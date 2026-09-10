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
