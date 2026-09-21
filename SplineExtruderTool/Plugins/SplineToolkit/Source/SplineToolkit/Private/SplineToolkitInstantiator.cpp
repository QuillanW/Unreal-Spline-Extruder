// Fill out your copyright notice in the Description page of Project Settings.

#include "SplineToolkitInstantiator.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Misc/Zip.h"

// Sets default values for this component's properties
USplineToolkitInstantiator::USplineToolkitInstantiator()
{
	// Set this component to be initialized when the game starts, and to be ticked
	// every frame.  You can turn these features off to improve performance if you
	// don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void USplineToolkitInstantiator::BeginPlay()
{
	Super::BeginPlay();

	// ...
}


void USplineToolkitInstantiator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}


FSplineToolkitRmfSample USplineToolkitInstantiator::GetRMFSampleAtDistance(
	float Distance, FSplineToolkitRmfSample& PrevSample) const
{
	if (Distance <= 0.0f)
	{
		auto NewSample = PrevSample;
		const float Roll = -SplineComponent->GetRollAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
		NewSample.Reference = NewSample.Reference.RotateAngleAxis(Roll, NewSample.Tangent);
		NewSample.Bitangent = NewSample.Tangent.Cross(NewSample.Reference);
		return NewSample;
	}

	const FVector Position = this->SplineComponent->GetLocationAtDistanceAlongSpline(
		Distance, ESplineCoordinateSpace::World);
	const FVector Tangent = this->SplineComponent->GetTangentAtDistanceAlongSpline(
		Distance, ESplineCoordinateSpace::World).GetSafeNormal();

	// Perform the first reflection R_1
	// Algorithm from https://dl.acm.org/doi/epdf/10.1145/1330511.1330513
	// Page 7, Table I
	const FVector Reflection1 = Position - PrevSample.Position;
	const float Reflection1SqrLength = Reflection1.SquaredLength();
	const FVector PrevReferenceLeftHanded = PrevSample.Reference - (2.0f / Reflection1SqrLength) * Reflection1.
		Dot(PrevSample.Reference) * Reflection1;
	const FVector PrevTangentLeftHanded = PrevSample.Tangent - (2.0f / Reflection1SqrLength) * Reflection1.
		Dot(PrevSample.Tangent) * Reflection1;

	const FVector Reflection2 = Tangent - PrevTangentLeftHanded;
	const float Reflection2SqrLength = Reflection2.SquaredLength();
	const FVector NewReference = PrevReferenceLeftHanded - (2.0f / Reflection2SqrLength) * Reflection2.Dot(
		PrevReferenceLeftHanded) * Reflection2;

	const FVector NewBitangent = Tangent.Cross(NewReference);

	auto NewSample = FSplineToolkitRmfSample{
		.Position = Position,
		.Distance = Distance,
		.Tangent = Tangent,
		.Bitangent = NewBitangent,
		.Reference = NewReference
	};

	PrevSample = NewSample;

	const float Roll = -SplineComponent->GetRollAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

	NewSample.Reference = NewSample.Reference.RotateAngleAxis(Roll, NewSample.Tangent);
	NewSample.Bitangent = NewSample.Tangent.Cross(NewSample.Reference);
	return NewSample;
}


void USplineToolkitInstantiator::OnRegister()
{
	Super::OnRegister();

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;

	if (AActor* Owner = GetOwner())
	{
		if (!Owner->FindComponentByClass<USplineComponent>())
		{
			UE_LOG(LogTemp, Error, TEXT("Instantiator requires USplineComponent"));
			return;
		}
		SplineComponent = Owner->GetComponentByClass<USplineComponent>();
		SplineComponent->GetOnSplineChanged().AddLambda([this] { if (AutoUpdate) Regenerate(); });
	}

	if (Ruleset->IsValidLowLevel())
	{
		Ruleset->OnShouldRegenerate.AddLambda([this]
		{
			if (AutoUpdate)
				Regenerate();
		});
		Ruleset->OnReapplyMaterials.AddLambda([this]
		{
			ReapplyMaterials();
		});
	}
}


void USplineToolkitInstantiator::RegenerateInternal()
{
	bRegenerate = false;

	// Easier to just clear all and regenerate since the components are small and quick to load 
	// And I'm kinda lazy while writing this at midnight...
	Clear();

	if (!this->Ruleset->IsValidLowLevelFast())
		return;

	// Get total length to step over
	const auto TotalLen = SplineComponent->GetSplineLength();

	// Keep the last RMF Sample
	FSplineToolkitRmfSample LastSample = {
		.Position = this->SplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World),
		.Distance = 0.0f,
		.Tangent = this->SplineComponent->GetTangentAtSplinePoint(0, ESplineCoordinateSpace::World).GetSafeNormal(),
		.Reference = this->SplineComponent->GetUpVectorAtSplinePoint(0, ESplineCoordinateSpace::World).GetSafeNormal(),
	};

	int32 RuleIdx = 0;

	// Go over each rule
	for (const auto& Rule : Ruleset->InstantiationRules)
	{
		// Loop over the spline at a set distance of precision. Applying the rules at each point
		for (float CurrentDist = 0.0f; CurrentDist <= TotalLen; CurrentDist += fmax(Rule.StepPrecision, 1.0f))
		{
			// TODO: Apply modifiers
			const auto ModdedRule = Rule;

			// Check if enabled (Can be changed by modifier, so checking each step)
			if (!ModdedRule.Enabled)
				continue;

			// Check if spacing is reached
			const float offset = fmodf(CurrentDist, ModdedRule.Spacing);
			if (offset >= ModdedRule.StepPrecision)
				continue;

			FSplineToolkitRmfSample Sample = GetRMFSampleAtDistance(CurrentDist, LastSample);

			FVector Pos = Sample.Position;
			FVector Rht = Sample.Bitangent;
			FVector Fwd = Sample.Tangent;
			FVector Up = Sample.Reference;
			FRotator Rot = FRotationMatrix::MakeFromXZ(Fwd, Up).Rotator();

			TObjectPtr<AActor> InstancerActor = {};
			if (RuleIdx >= SpawnedInstancedMeshes.Num())
			{
				InstancerActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Pos, Rot);
#if WITH_EDITOR
				InstancerActor->SetActorLabel("SplineInstantiatorInstancer" + FString::FromInt(RuleIdx));
#endif
				if (InstancerActor)
				{
					SpawnedInstancedMeshes.Add(InstancerActor);
					UInstancedStaticMeshComponent* NewMeshComp = NewObject<UInstancedStaticMeshComponent>(
						InstancerActor);
					NewMeshComp->SetMaterial(0, Rule.Material);
					NewMeshComp->SetOverlayMaterial(Rule.OverlayMaterial);
					NewMeshComp->RegisterComponent();
					InstancerActor->SetRootComponent(NewMeshComp);
					InstancerActor->AttachToActor(this->GetOwner(),
					                              FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				}
			}
			else
			{
				InstancerActor = SpawnedInstancedMeshes[RuleIdx];
			}

			if (InstancerActor)
			{
				FVector Offset = Rht * ModdedRule.Offset.X;
				Offset += Fwd * ModdedRule.Offset.Y;
				Offset += Up * ModdedRule.Offset.Z;

				FTransform Transform{Rot, Pos + Offset, ModdedRule.Scale};

				if (auto InstancerComp = InstancerActor->GetComponentByClass<UInstancedStaticMeshComponent>())
				{
					InstancerComp->SetStaticMesh(ModdedRule.Mesh);
					InstancerComp->AddInstance(Transform, true);
				}
			}
		}

		++RuleIdx;
	}
}


void USplineToolkitInstantiator::ReapplyMaterials()
{
	for (const auto& [Rule, Actor] : UE::Zip(this->Ruleset->InstantiationRules, this->SpawnedInstancedMeshes))
	{
		auto* Comp = Actor->GetComponentByClass<UInstancedStaticMeshComponent>();
		Comp->SetMaterial(0, Rule.Material);
		Comp->SetOverlayMaterial(Rule.OverlayMaterial);
	}
}


void USplineToolkitInstantiator::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	Clear();
}


// Called every frame
void USplineToolkitInstantiator::TickComponent(
	float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bRegenerate)
		RegenerateInternal();
}


void USplineToolkitInstantiator::Regenerate()
{
	bRegenerate = true;
}


void USplineToolkitInstantiator::Clear()
{
	// Clean up old objects
	for (const auto& actor : SpawnedInstancedMeshes)
		if (IsValid(actor))
			actor->Destroy();

	SpawnedInstancedMeshes.Empty();
}


void USplineToolkitInstantiator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (AutoUpdate)
		Regenerate();

	if (Ruleset->IsValidLowLevel())
	{
		Ruleset->OnShouldRegenerate.AddLambda([this]
		{
			if (AutoUpdate)
				Regenerate();
		});
		Ruleset->OnReapplyMaterials.AddLambda([this]
		{
			ReapplyMaterials();
		});
	}
}
