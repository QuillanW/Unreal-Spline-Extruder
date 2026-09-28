// Fill out your copyright notice in the Description page of Project Settings.

#include "SplineToolkitInstantiator.h"

#include "SplineToolkitModifier.h"
#include "SplineToolkitIntersectionSolver.h"
#include "SplineToolkitRmf.h"
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

USplineToolkitRulesetModifierBase* USplineToolkitInstantiator::GetOrCreateModifierInstance(TSubclassOf<USplineToolkitRulesetModifierBase> Class)
{
	if (USplineToolkitRulesetModifierBase** Found = ModifierInstanceCache.Find(Class))
	{
		return *Found;
	}
	USplineToolkitRulesetModifierBase* NewInstance = NewObject<USplineToolkitRulesetModifierBase>(GetTransientPackage(), Class);
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
			FSplineToolkitStepContext Context {SplineComponent, CurrentDist};

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
			if (const auto* Solver = GetOwner()->FindComponentByClass<USplineToolkitIntersectionSolver>())
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
				FTransform Transform;
				FVector FinalPosition = Sample.Position + Rotation.TransformPosition(Rule.Offset);
				Transform.SetComponents(Rotation.ToQuat().GetNormalized(), FinalPosition, Rule.Scale);

				if (auto InstancerComp = InstancerActor->GetComponentByClass<UInstancedStaticMeshComponent>())
				{
					InstancerComp->SetStaticMesh(ModdedRule.Mesh);
					InstancerComp->AddInstance(Transform, false);
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
