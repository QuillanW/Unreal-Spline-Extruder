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
	bool Retry = bRetry;
	bRetry = false;
	
	if (!this->Ruleset->IsValidLowLevelFast())
	{
		UE_LOG(LogTemp, Error, TEXT("No ruleset specified"));
		return;
	}
	
	if (!this->InstantiatorComponent->IsValidLowLevelFast())
	{
		UE_LOG(LogTemp, Error, TEXT("Stretcher requires USplineToolkitInstantiator"));
		return;
	}
	
	// Easier to just clear all and regenerate since the components are small and quick to load 
	// And I'm kinda lazy while writing this at midnight...
	Clear();
	
	// Get all anchors from the instantiator
	TMap<FName, TArray<FVector>> Anchors;
	for (const auto& Anchor : InstantiatorComponent->Anchors)
		Anchors.FindOrAdd(Anchor.Type).Add(Anchor.SpawnedLocalLocation);
	
	TMap<int32, int32> AnchorConnectCount = {};
	
	if (Anchors.IsEmpty())
		if (!Retry) bRetry = true;
	
	// Loop over the rules
	int32 RuleIdx = -1;
	for (const auto& Rule : Ruleset->StretchRules)
	{
		if (!Anchors.Contains(Rule.StartAnchorType)) continue;
		if (Rule.ConnectionType == ESplineToolkitStretchConnectionType::Closest)
			if (!Anchors.Contains(Rule.EndAnchorType)) continue;
		
		RuleIdx++;
		TArray<FMeshStretcherInstance> MeshInstances = {};
		
		if (Rule.ConnectionType == ESplineToolkitStretchConnectionType::Closest)
		{
			for (int32 StartIdx = 0; StartIdx < Anchors[Rule.StartAnchorType].Num() - 1; StartIdx++)
			{
				FVector StartPoint = Anchors[Rule.StartAnchorType][StartIdx];
				float ClosestDist = Rule.MaxDistance;
				int32 AnchorIdx = -1;
				for (int32 EndIdx = 0; EndIdx < Anchors[Rule.EndAnchorType].Num() - 1; EndIdx++)
				{
					if (!AnchorConnectCount.Contains(EndIdx)) 
						AnchorConnectCount.Add(EndIdx) = 0;
					
					FVector Offset = Anchors[Rule.EndAnchorType][EndIdx] - StartPoint;
					float Dist = Offset.Length();
					if (Dist > ClosestDist || Dist < Rule.MinDistance || AnchorConnectCount[EndIdx] >= Rule.MaxConnectCount) continue;
					ClosestDist = Dist;
					AnchorIdx = EndIdx;
					AnchorConnectCount[EndIdx]++;
				}
				
				if (AnchorIdx != -1)
					MeshInstances.Add({StartPoint, Anchors[Rule.EndAnchorType][AnchorIdx]});
			}
		}
		else if (Rule.ConnectionType == ESplineToolkitStretchConnectionType::Sequential)
		{
			for (int32 Idx = 0; Idx < Anchors[Rule.StartAnchorType].Num() - 1; Idx++)
			{
				MeshInstances.Add({Anchors[Rule.StartAnchorType][Idx], Anchors[Rule.StartAnchorType][Idx + 1]});
			}
		}
		
		TObjectPtr<AActor> InstancerActor = {};
		
		if (RuleIdx >= SpawnedInstancedMeshes.Num())
		{
			InstancerActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());
#if WITH_EDITOR
			InstancerActor->SetActorLabel("SplineStretchInstancer" + FString::FromInt(RuleIdx));
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
			
		if (!InstancerActor) continue;
		
		for (const auto& Instance : MeshInstances)
		{
			FVector PointA = Instance.StartPos;
			FVector PointB = Instance.EndPos;
			FVector Direction = PointB - PointA;
			FVector Center = PointA + Direction * 0.5f;
			double Distance = Direction.Length();
						
			double Size = Rule.Mesh->GetBoundingBox().GetSize().Y;
			double Scale = Distance / Size;
			FVector BoxCenter = Rule.Mesh->GetBoundingBox().GetCenter();
						
			// Apply rotation from the rule and offset by 90 degrees (Otherwise model is in wrong axis)
			const FQuat Rotation =
				Direction.Rotation().Quaternion()
				* FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Rule.bRollOffset))
				* FRotator(0.f, -90.f, 0.f).Quaternion();
						
			FVector Scale3D = {Rule.Scale.X, Scale, Rule.Scale.Y};
						
			FTransform Transform;
			Transform.SetLocation(Center - Rotation.RotateVector(BoxCenter * Scale3D));
			Transform.SetRotation(Rotation);
			Transform.SetScale3D(Scale3D);

			if (auto InstancerComp = InstancerActor->GetComponentByClass<UInstancedStaticMeshComponent>())
			{
				InstancerComp->SetStaticMesh(Rule.Mesh);
				InstancerComp->AddInstance(Transform, false);
			}
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
