// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Runtime/Engine/Classes/Components/StaticMeshComponent.h"
#include "SplineToolkitMeshExtruder.generated.h"

/**
 * This is an alternative to USplineMeshComponent that allows the use of SplineToolkit's modifiers and intersection rules
 */
UCLASS(ClassGroup=(Custom),
	meta=(BlueprintSpawnableComponent, ToolTip="A component that extrudes a given mesh along a given spline"))
class SPLINETOOLKIT_API USplineToolkitMeshExtruder : public UStaticMeshComponent
{
	GENERATED_BODY()

public:

	virtual void OnRegister() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(CallInEditor)
	void RecalculateMesh();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	USplineComponent* SplineComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumRmfSamples = 128;

private:

	struct RMFSample
	{
		FVector Position;
		FVector Tangent; // Front vector
		FVector Bitangent; // Right vector
		FVector Reference; // Up vector
	};

#ifdef WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	TArray<RMFSample> RmfSamples;

	void RecalculateRmfSamples();

};
