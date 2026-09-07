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

void USplineToolkitInstantiator::OnRegister()
{
	Super::OnRegister();

	if (AActor* Owner = GetOwner())
	{
		if (!Owner->FindComponentByClass<USplineComponent>())
		{
			UE_LOG(LogTemp, Error, TEXT("Instantiator requires USplineComponent"));
			return;
		}
		SplineComponent = Owner->GetComponentByClass<USplineComponent>();
	}
}

// Called every frame
void USplineToolkitInstantiator::TickComponent(
	float                        DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void USplineToolkitInstantiator::Regenerate()
{
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

#if WITH_EDITOR
				NewActor->SetFolderPath(FName("Instanced Meshes"));
#endif
			}
		}
		
	}
}

void USplineToolkitInstantiator::Clear()
{
	// Clean up old objects
	for (const auto& actor : SpawnedInstancedMeshes)
		actor->Destroy();
	
	SpawnedInstancedMeshes.Empty();
}
