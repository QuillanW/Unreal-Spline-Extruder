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
	for (const auto& actor : SpawnedInstancedMeshes)
		actor->Destroy();
	
	SpawnedInstancedMeshes.Empty();
	
	// Go over each rule
	for (const auto& rule : Ruleset->InstantiationRules)
	{
		// Loop over the spline at a set distance of precision. Applying the rules at each point
		const auto length = SplineComponent->GetSplineLength();
		
		for (float current = 0.0f; current <= length; current += rule.Spacing)
		{
			const auto pos = SplineComponent->GetWorldLocationAtDistanceAlongSpline(current);
			const auto rot = SplineComponent->GetWorldRotationAtDistanceAlongSpline(current);

			FActorSpawnParameters SpawnParams;
			AActor* NewActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), pos, rot, SpawnParams);

			if (NewActor)
			{
				SpawnedInstancedMeshes.Add(NewActor);
				
				UStaticMeshComponent* NewMeshComp = NewObject<UStaticMeshComponent>(NewActor);
				NewMeshComp->SetStaticMesh(rule.Mesh);
				NewMeshComp->RegisterComponent();
				NewActor->SetRootComponent(NewMeshComp);
				NewActor->SetActorLocationAndRotation(pos, rot);
				
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
