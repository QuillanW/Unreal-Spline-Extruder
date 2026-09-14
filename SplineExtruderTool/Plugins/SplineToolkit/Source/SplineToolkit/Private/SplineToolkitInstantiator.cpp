// Fill out your copyright notice in the Description page of Project Settings.

#include "SplineToolkitInstantiator.h"

#include "Components/SplineComponent.h"

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

void USplineToolkitInstantiator::RecalculateRmfSamples(int32 NumRmfSamples, FSplineToolkitExtruderMeshData& Data) const
{
	// Perform simple RMF for now
	Data.RmfSamples.Empty();
	Data.RmfSamples.Reserve(NumRmfSamples);

	// 0th sample is the first tangent
	FSplineToolkitRmfSample PrevSample = {
		.Position = this->SplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local),
		.Distance = 0.0f,
		.Tangent = this->SplineComponent->GetTangentAtSplinePoint(0, ESplineCoordinateSpace::Local).GetSafeNormal(),
		.Reference = this->SplineComponent->GetUpVectorAtSplinePoint(0, ESplineCoordinateSpace::Local).GetSafeNormal(),
	};
	PrevSample.Bitangent = PrevSample.Tangent.Cross(PrevSample.Reference);

	Data.RmfSamples.Add(PrevSample);

	for (int32 SampleIter = 1; SampleIter < NumRmfSamples; ++SampleIter)
	{
		const float Time = SampleIter / static_cast<float>(NumRmfSamples - 1);

		const FVector Position = this->SplineComponent->GetLocationAtTime(Time, ESplineCoordinateSpace::Local);
		const FVector Tangent = this->SplineComponent->GetTangentAtTime(Time, ESplineCoordinateSpace::Local).
		                              GetSafeNormal();
		const float Distance = this->SplineComponent->GetDistanceAlongSplineAtLocation(
			Position, ESplineCoordinateSpace::Local);

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
		Data.RmfSamples.Add(NewSample);
		PrevSample = NewSample;
	}

	// Apply roll
	for (auto& Sample : Data.RmfSamples)
	{
		const float Roll = -SplineComponent->GetRollAtDistanceAlongSpline(Sample.Distance, ESplineCoordinateSpace::Local);

		Sample.Reference = Sample.Reference.RotateAngleAxis(Roll, Sample.Tangent);
		Sample.Bitangent = Sample.Tangent.Cross(Sample.Reference);
	}
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
		SplineComponent->GetOnSplineChanged().AddLambda([this]{ if (AutoUpdate) Regenerate(); });
	}
	
	if (Ruleset->IsValidLowLevel())
		Ruleset->OnChanged.AddLambda([this]
		{
			if (AutoUpdate)
				Regenerate();
		});
}

void USplineToolkitInstantiator::RegenerateInternal()
{
	bRegenerate = false;
	
	if (!this->Ruleset->IsValidLowLevelFast())
	    return;
	
	// Clean up old objects
	for (const auto& Actor : SpawnedInstancedMeshes)
		Actor->Destroy();

	SpawnedInstancedMeshes.Empty();

	// Get total length to step over
	const auto TotalLen = SplineComponent->GetSplineLength();

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

			const auto Pos = SplineComponent->GetWorldLocationAtDistanceAlongSpline(CurrentDist);
			const auto Rot = SplineComponent->GetWorldRotationAtDistanceAlongSpline(CurrentDist);

			const auto Fwd = SplineComponent->GetDirectionAtDistanceAlongSpline(
				CurrentDist, ESplineCoordinateSpace::World);
			const auto Rht = SplineComponent->GetRightVectorAtDistanceAlongSpline(
				CurrentDist, ESplineCoordinateSpace::World);
			const auto Up = SplineComponent->GetUpVectorAtDistanceAlongSpline(
				CurrentDist, ESplineCoordinateSpace::World);

			FActorSpawnParameters SpawnParams;
			AActor* NewActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Pos, Rot, SpawnParams);

			if (NewActor)
			{
				SpawnedInstancedMeshes.Add(NewActor);

				FVector Offset = Rht * ModdedRule.Offset.X;
				Offset += Fwd * ModdedRule.Offset.Y;
				Offset += Up * ModdedRule.Offset.Z;
				
				UStaticMeshComponent* NewMeshComp = NewObject<UStaticMeshComponent>(NewActor);
				NewMeshComp->SetStaticMesh(ModdedRule.Mesh);
				NewMeshComp->RegisterComponent();
				NewActor->SetRootComponent(NewMeshComp);
				NewActor->SetActorLocationAndRotation(Pos + Offset, Rot);
				NewActor->SetActorScale3D(ModdedRule.Scale);

				NewActor->AttachToActor(this->GetOwner(), FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}
}

void USplineToolkitInstantiator::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
	
	Clear();
}

// Called every frame
void USplineToolkitInstantiator::TickComponent(
	float                        DeltaTime, ELevelTick TickType,
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
		actor->Destroy();

	SpawnedInstancedMeshes.Empty();
}

void USplineToolkitInstantiator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (AutoUpdate)
		Regenerate();
	
	if (Ruleset->IsValidLowLevel())
		Ruleset->OnChanged.AddLambda([this]
		{
			if (AutoUpdate)
				Regenerate();
		});
}
