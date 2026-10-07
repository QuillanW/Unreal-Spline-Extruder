// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "Components/SplineComponent.h"
#include "CoreMinimal.h"
#include "SplineToolkitRuleset.h"
#include "SplineToolkitMeshStretcher.generated.h"

class USplineToolkitInstantiator;

struct FMeshStretcherInstance
{
	FVector StartPos;
	FVector EndPos;
};


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), DisplayName = "[Spline Toolkit] Mesh Stretcher")
class SPLINETOOLKIT_API USplineToolkitMeshStretcher : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	USplineToolkitMeshStretcher();

	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	
protected:
	virtual void OnRegister() override;
	
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
	TObjectPtr<USplineComponent> SplineComponent = {};
	
	UPROPERTY(BlueprintReadOnly, Category = "Spline Toolkit")
	TObjectPtr<USplineToolkitInstantiator> InstantiatorComponent = {};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	bool AutoUpdate = true;
	
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedInstancedMeshes = {};
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
