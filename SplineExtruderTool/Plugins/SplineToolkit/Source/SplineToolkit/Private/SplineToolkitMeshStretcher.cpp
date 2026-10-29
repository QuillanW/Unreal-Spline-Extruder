// Fill out your copyright notice in the Description page of Project Settings.

#include "SplineToolkitMeshStretcher.h"

#include "SplineToolkitInstantiator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/Zip.h"

// Sets default values for this component's properties
USplineToolkitMeshStretcher::USplineToolkitMeshStretcher()
{
	// Set this component to be initialized when the game starts, and to be ticked
	// every frame.  You can turn these features off to improve performance if you
	// don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void USplineToolkitMeshStretcher::BeginPlay()
{
	Super::BeginPlay();

	// ...
}


void USplineToolkitMeshStretcher::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}


void USplineToolkitMeshStretcher::OnRegister()
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
			UE_LOG(LogTemp, Error, TEXT("Stretcher requires USplineComponent"));
			return;
		}
		SplineComponent = Owner->GetComponentByClass<USplineComponent>();
		SplineComponent->GetOnSplineChanged().AddLambda([this] { if (AutoUpdate) Regenerate(); });
		
		if (!Owner->FindComponentByClass<USplineToolkitInstantiator>())
		{
			UE_LOG(LogTemp, Error, TEXT("Stretcher requires USplineToolkitInstantiator"));
			return;
		}
		InstantiatorComponent = Owner->GetComponentByClass<USplineToolkitInstantiator>();
	}

	if (IsValid(Ruleset))
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

void USplineToolkitMeshStretcher::RegenerateInternal()
{
	bRegenerate = false;
	const bool Retry = bRetry;
	bRetry = false;

	if (!IsValid(Ruleset))
	{
		UE_LOG(LogTemp, Error, TEXT("No ruleset specified"));
		return;
	}

	if (!IsValid(InstantiatorComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("Stretcher requires USplineToolkitInstantiator"));
		return;
	}

	Clear();

	// Get all anchors from the instantiator
	TMap<FName, TArray<FVector>> Anchors;
	for (const auto& Anchor : InstantiatorComponent->Anchors)
		Anchors.FindOrAdd(Anchor.Type).Add(Anchor.SpawnedLocalLocation);

	if (Anchors.IsEmpty() && !Retry)
		bRetry = true;

	int32 RuleIdx = -1;
	for (const auto& Rule : Ruleset->StretchRules)
	{
		if (!Anchors.Contains(Rule.StartAnchorType)) continue;
		if (Rule.ConnectionType == ESplineToolkitStretchConnectionType::Closest &&
			!Anchors.Contains(Rule.EndAnchorType)) continue;
		if (!IsValid(Rule.Mesh)) continue;

		RuleIdx++;

		// Size of the mesh along the stretch axis, and the rotation that maps that mesh axis onto local X
		const FBox MeshBox = Rule.Mesh->GetBoundingBox();
		const FVector MeshSize = MeshBox.GetSize();
		const FVector BoxCenter = MeshBox.GetCenter();

		double Size = 1.0;
		FQuat AxisAlign = FQuat::Identity;
		switch (Rule.StretchAxis)
		{
		case ESplineToolkitAxis::X:
			Size = MeshSize.X;
			AxisAlign = FQuat::Identity;
			break;
		case ESplineToolkitAxis::Y:
			Size = MeshSize.Y;
			AxisAlign = FRotator(0.f, -90.f, 0.f).Quaternion(); // Y -> X
			break;
		case ESplineToolkitAxis::Z:
			Size = MeshSize.Z;
			AxisAlign = FRotator(-90.f, 0.f, 0.f).Quaternion(); // Z -> X
			break;
		}
		if (Size <= KINDA_SMALL_NUMBER) continue;

		// Build the connections
		TArray<FMeshStretcherInstance> MeshInstances = {};
		const TArray<FVector>& StartAnchors = Anchors[Rule.StartAnchorType];

		if (Rule.ConnectionType == ESplineToolkitStretchConnectionType::Closest)
		{
			const TArray<FVector>& EndAnchors = Anchors[Rule.EndAnchorType];
			TMap<int32, int32> ConnectCount; // per rule

			for (int32 StartIdx = 0; StartIdx < StartAnchors.Num(); StartIdx++)
			{
				const FVector StartPoint = StartAnchors[StartIdx];
				double ClosestDist = Rule.MaxDistance;
				int32 AnchorIdx = INDEX_NONE;

				for (int32 EndIdx = 0; EndIdx < EndAnchors.Num(); EndIdx++)
				{
					const double Dist = (EndAnchors[EndIdx] - StartPoint).Length();
					if (Dist > ClosestDist || Dist < Rule.MinDistance) continue;
					if (ConnectCount.FindOrAdd(EndIdx) >= Rule.MaxConnectCount) continue;
					ClosestDist = Dist;
					AnchorIdx = EndIdx;
				}

				if (AnchorIdx != INDEX_NONE)
				{
					ConnectCount[AnchorIdx]++;
					MeshInstances.Add({StartPoint, EndAnchors[AnchorIdx]});
				}
			}
		}
		else if (Rule.ConnectionType == ESplineToolkitStretchConnectionType::Sequential)
		{
			for (int32 Idx = 0; Idx < StartAnchors.Num() - 1; Idx++)
				MeshInstances.Add({StartAnchors[Idx], StartAnchors[Idx + 1]});
		}

		// Create the instancer actor for this rule
		AActor* InstancerActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());
		if (!InstancerActor) continue;
#if WITH_EDITOR
		InstancerActor->SetActorLabel("SplineStretchInstancer" + FString::FromInt(SpawnedInstancedMeshes.Num()));
#endif
		SpawnedInstancedMeshes.Add(InstancerActor);

		auto* InstancerComp = NewObject<UInstancedStaticMeshComponent>(InstancerActor);
		InstancerComp->SetStaticMesh(Rule.Mesh);
		InstancerComp->SetMaterial(0, Rule.Material);
		InstancerComp->SetOverlayMaterial(Rule.OverlayMaterial);
		InstancerComp->RegisterComponent();
		InstancerActor->SetRootComponent(InstancerComp);
		InstancerActor->AttachToActor(GetOwner(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		
		if (!InstancerActor) continue;
		if (!InstancerComp) continue;
		
		InstancerComp->SetStaticMesh(Rule.Mesh);

		// Place the instances
		for (const auto& Instance : MeshInstances)
		{
			const FVector Direction = Instance.EndPos - Instance.StartPos;
			const double Distance = Direction.Length();
			if (Distance <= KINDA_SMALL_NUMBER) continue;

			const FVector Center = Instance.StartPos + Direction * 0.5;
			const double Scale = Distance / Size;

			const FQuat Rotation =
				Direction.Rotation().Quaternion()
				* FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Rule.bRollOffset))
				* AxisAlign;

			FVector Scale3D = FVector::OneVector;
			switch (Rule.StretchAxis)
			{
			case ESplineToolkitAxis::X: Scale3D = FVector(Scale,        Rule.Scale.X, Rule.Scale.Y); break;
			case ESplineToolkitAxis::Y: Scale3D = FVector(Rule.Scale.X, Scale,        Rule.Scale.Y); break;
			case ESplineToolkitAxis::Z: Scale3D = FVector(Rule.Scale.X, Rule.Scale.Y, Scale);        break;
			}

			FTransform Transform;
			Transform.SetLocation(Center - Rotation.RotateVector(BoxCenter * Scale3D));
			Transform.SetRotation(Rotation);
			Transform.SetScale3D(Scale3D);

			InstancerComp->AddInstance(Transform, false);
		}
	}
}


void USplineToolkitMeshStretcher::ReapplyMaterials()
{
	for (const auto& [Rule, Actor] : UE::Zip(this->Ruleset->StretchRules, this->SpawnedInstancedMeshes))
	{
		if (!IsValid(Actor)) continue;
		auto* Comp = Actor->GetComponentByClass<UInstancedStaticMeshComponent>();
		Comp->SetMaterial(0, Rule.Material);
		Comp->SetOverlayMaterial(Rule.OverlayMaterial);
	}
}


void USplineToolkitMeshStretcher::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	Clear();
}


// Called every frame
void USplineToolkitMeshStretcher::TickComponent(
	float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bRegenerate || bRetry)
		RegenerateInternal();
}


void USplineToolkitMeshStretcher::Regenerate()
{
	bRegenerate = true;
}


void USplineToolkitMeshStretcher::Clear()
{
	// Clean up old objects
	for (const auto& actor : SpawnedInstancedMeshes)
		if (IsValid(actor))
			actor->Destroy();

	SpawnedInstancedMeshes.Empty();
}


void USplineToolkitMeshStretcher::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (AutoUpdate)
		Regenerate();

	if (IsValid(Ruleset))
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
