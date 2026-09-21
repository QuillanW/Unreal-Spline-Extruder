// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "Components/SplineComponent.h"
#include "CoreMinimal.h"
#include "SplineToolkitRuleset.h"
#include "SplineToolkitInstantiator.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), DisplayName = "[Spline Toolkit] Mesh Instantiator")
class SPLINETOOLKIT_API USplineToolkitInstantiator : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	USplineToolkitInstantiator();

	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	
protected:
	virtual void OnRegister() override;
	
	FSplineToolkitRmfSample GetRMFSampleAtDistance(float d, FSplineToolkitRmfSample& prevSample) const;
	
	void RegenerateInternal();
	void ReapplyMaterials();
	
	bool bRegenerate = false;

public:
	// Called every frame
	virtual void
	TickComponent(float                        DeltaTime, ELevelTick TickType,
	              FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Spline Toolkit")
	void Regenerate();
	
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Spline Toolkit")
	void Clear();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	USplineToolkitRuleset* Ruleset = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	USplineComponent* SplineComponent = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	bool AutoUpdate = true;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	TArray<TObjectPtr<AActor>> SpawnedInstancedMeshes = {};
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
