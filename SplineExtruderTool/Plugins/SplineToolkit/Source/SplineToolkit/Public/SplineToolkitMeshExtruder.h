// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Runtime/Engine/Classes/Components/StaticMeshComponent.h"
#include "SplineToolkitMeshExtruder.generated.h"

/**
 * This is an alternative to USplineMeshComponent that allows the use of SplineToolkit's modifiers and intersection rules
 */
UCLASS(ClassGroup=(Custom),
	meta=(BlueprintSpawnableComponent, ToolTip="A component that extrudes a given mesh along a given spline"))
class SPLINETOOLKIT_API USplineToolkitMeshExtruder : public UActorComponent
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UStaticMesh* InputMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumRmfSamples = 128;

private:

	struct FRmfSample
	{
		FVector Position;
		float Distance;
		FVector Tangent;   // Front vector
		FVector Bitangent; // Right vector
		FVector Reference; // Up vector
	};

#ifdef WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	TArray<FRmfSample> RmfSamples;

	struct FExtruderDrawData
	{
		TArray<FVector> Positions;
		TArray<FVector> Normals;
		TArray<FVector2D> Uv0;
		TArray<FProcMeshTangent> Tangents;

		FExtruderDrawData() = default;
		void Reserve(uint32 NumVertices);
		void Init(uint32 NumVertices);

		void Insert(const FExtruderDrawData& Other, uint32 Where);

		uint32 Num() const;
	};

	FExtruderDrawData OriginSlice;

	// The output mesh
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> OutMesh;

	void RecalculateRmfSamples();

	void ExtractOriginSlice();

	void ComputeMesh();

};
