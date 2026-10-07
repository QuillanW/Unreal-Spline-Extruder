// Fill out your copyright notice in the Description page of Project Settings.

#include "SplineToolkitInstantiator.h"

#include "SplineToolkitModifier.h"
#include "SplineToolkitIntersectionSolver.h"
#include "SplineToolkitRmf.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Misc/Zip.h"

int32 GSplineToolkitShowAnchors = 0;
static FAutoConsoleVariableRef CVarShowAnchors(
	TEXT("stk.ShowAnchors"),
	GSplineToolkitShowAnchors,
	TEXT(
		"Shows debug spheres where anchors are placed on any rule"));

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


USplineToolkitRulesetModifierBase* USplineToolkitInstantiator::GetOrCreateModifierInstance(
	TSubclassOf<USplineToolkitRulesetModifierBase> Class)
{
	if (USplineToolkitRulesetModifierBase** Found = ModifierInstanceCache.Find(Class))
	{
		return *Found;
	}
	USplineToolkitRulesetModifierBase* NewInstance = NewObject<USplineToolkitRulesetModifierBase>(
		GetTransientPackage(), Class);
	ModifierInstanceCache.Add(Class, NewInstance);
	return NewInstance;
}


void USplineToolkitInstantiator::RegenerateInternal()
{
	bRegenerate = false;

	// Easier to just clear all and regenerate since the components are small and quick to load 
	// And I'm kinda lazy while writing this at midnight...
	Clear();

	if (!this->Ruleset->IsValidLowLevelFast())
		return;

	const auto* Solver = (this->bIgnoreIntersectCutouts)
		                     ? nullptr
		                     : GetOwner()->FindComponentByClass<USplineToolkitIntersectionSolver>();

	auto* RmfSampler = GetOwner()->FindComponentByClass<USplineToolkitRmfSampler>();
	if (!RmfSampler)
		return;

	// Get total length to step over
	const auto TotalLen = this->SplineComponent->GetSplineLength();

	int32 RuleIdx = 0;

	// Go over each rule
	for (const auto& Rule : this->Ruleset->InstantiationRules)
	{
		// Loop over the spline at a set distance of precision. Applying the rules at each point
		for (float CurrentDist = 0.0f; CurrentDist <= TotalLen; CurrentDist += fmax(Rule.StepPrecision, 1.0f))
		{
			FSplineToolkitStepContext Context{SplineComponent, CurrentDist};

			FSplineToolkitInstantiationRule ModdedRule = Rule;
			for (USplineToolkitRulesetModifierBase* Modifier : Rule.Modifiers)
			{
				if (!Modifier) continue;
				ModdedRule = Modifier->ModifyInstantiationStep(Context, ModdedRule);
			}

			// Check if enabled (Can be changed by modifier, so checking each step)
			if (!ModdedRule.Enabled)
				continue;

			// Check if it's in an intersection
			if (Solver)
			{
				bool bFound = false;
				for (const auto& Collision : Solver->Collisions)
				{
					if (FMath::IsWithin(CurrentDist, Collision.DistanceMin, Collision.DistanceMax))
					{
						bFound = true;
						break;
					}
				}
				if (bFound)
					continue;
			}

			// Check if spacing is reached
			const float Offset = fmodf(CurrentDist, ModdedRule.Spacing);
			if (Offset >= ModdedRule.StepPrecision)
				continue;

			auto Sample = RmfSampler->GetSampleAtDistance(CurrentDist);

			TObjectPtr<AActor> InstancerActor = {};
			if (RuleIdx >= SpawnedInstancedMeshes.Num())
			{
				InstancerActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());
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
				FMatrix Rotation{
					Sample.Bitangent.GetSafeNormal(),
					Sample.Tangent.GetSafeNormal(),
					Sample.Reference.GetSafeNormal(),
					FVector::ZeroVector
				};
				
				FQuat FinalRotation = Rotation.ToQuat() * ModdedRule.RotationOffset.Quaternion();
				
				FTransform Transform;
				FVector FinalPosition = Sample.Position + Rotation.TransformPosition(ModdedRule.Offset);
				Transform.SetComponents(FinalRotation.GetNormalized(), FinalPosition, ModdedRule.Scale);

				if (auto InstancerComp = InstancerActor->GetComponentByClass<UInstancedStaticMeshComponent>())
				{
					InstancerComp->SetStaticMesh(ModdedRule.Mesh);
					InstancerComp->AddInstance(Transform, false);
				}
				
				for (auto Anchor : ModdedRule.Anchors)
				{
					auto PlacedAnchor = Anchor;
					PlacedAnchor.SpawnedLocalLocation = Sample.Position + Rotation.TransformPosition(PlacedAnchor.Offset);
					Anchors.Add(PlacedAnchor);
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
		if (!IsValid(Actor)) continue;
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
	
	FVector Offset = GetOwner()->GetActorTransform().GetLocation();
	
	if (GSplineToolkitShowAnchors)
		for (const auto& Anchor : Anchors)
			DrawDebugSphere(GetWorld(), Offset + Anchor.SpawnedLocalLocation, 10.0f, 8, FColor::White);
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
	Anchors.Empty();
}


void USplineToolkitInstantiator::MarkDirty()
{
	this->bRegenerate = true;
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
