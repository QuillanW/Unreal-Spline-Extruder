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

	// ...
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
			UE_LOG(LogTemp, Error, TEXT("Instantiator requires USplineComponent"));
			return;
		}
		SplineComponent = Owner->GetComponentByClass<USplineComponent>();
		SplineComponent->GetOnSplineChanged().AddLambda([this] { if (AutoUpdate) Regenerate(); });
		
		if (!Owner->FindComponentByClass<USplineToolkitInstantiator>())
		{
			UE_LOG(LogTemp, Error, TEXT("Instantiator requires USplineToolkitInstantiator"));
			return;
		}
		InstantiatorComponent = Owner->GetComponentByClass<USplineToolkitInstantiator>();
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

void USplineToolkitMeshStretcher::RegenerateInternal()
{
	bRegenerate = false;

	// Easier to just clear all and regenerate since the components are small and quick to load 
	// And I'm kinda lazy while writing this at midnight...
	Clear();

	if (!this->Ruleset->IsValidLowLevelFast())
		return;
	
	// Get all anchors from the instantiator
	TMap<FName, TArray<FVector>> Anchors;
	for (const auto& Anchor : InstantiatorComponent->Anchors)
		Anchors.FindOrAdd(Anchor.Type).Add(Anchor.SpawnedWorldLocation);
	
	if (Anchors.IsEmpty()) return;
	
	int32 RuleIdx = -1;
	
	// Loop over the rules
	for (const auto& Rule : Ruleset->StretchRules)
	{
		++RuleIdx;
		
		if (Rule.ConnectionType == ESplineToolkitStretchConnectionType::Closest) continue; // TODO
		
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
		
		for (int32 Idx = 0; Idx < Anchors[Rule.StartAnchorType].Num() - 1; Idx++)
		{
			FVector PointA = Anchors[Rule.StartAnchorType][Idx];
			FVector PointB = Anchors[Rule.StartAnchorType][Idx + 1];
			FVector Center = PointA + (PointB - PointA) * 0.5f;
			FVector Direction = PointB - PointA;
			double Distance = Direction.Length();
			double Size = Rule.Mesh->GetBoundingBox().GetSize().Y;
			double Scale = Distance / Size;
			
			FTransform Transform;
			Transform.SetLocation(Center);
			Transform.SetRotation(FRotationMatrix::MakeFromY(Direction).ToQuat());
			Transform.SetScale3D({1, Scale, 1});

			if (auto InstancerComp = InstancerActor->GetComponentByClass<UInstancedStaticMeshComponent>())
			{
				InstancerComp->SetStaticMesh(Rule.Mesh);
				InstancerComp->AddInstance(Transform, true);
			}
		}
		
	}
}


void USplineToolkitMeshStretcher::ReapplyMaterials()
{
	for (const auto& [Rule, Actor] : UE::Zip(this->Ruleset->InstantiationRules, this->SpawnedInstancedMeshes))
	{
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

	if (bRegenerate)
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
