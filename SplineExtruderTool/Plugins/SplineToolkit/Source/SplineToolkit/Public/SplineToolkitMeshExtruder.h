// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "SplineToolkitRuleset.h"
#include "Components/SplineComponent.h"
#include "Runtime/Engine/Classes/Components/StaticMeshComponent.h"
#include "SplineToolkitMeshExtruder.generated.h"

USTRUCT()
struct FSplineToolkitRmfSample
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position;
	UPROPERTY()
	float Distance;
	UPROPERTY()
	FVector Tangent;   // Front vector
	UPROPERTY()
	FVector Bitangent; // Right vector
	UPROPERTY()
	FVector Reference; // Up vector
};

USTRUCT()
struct FSplineToolkitExtruderDrawData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector> Positions;
	UPROPERTY()
	TArray<FVector> Normals;
	UPROPERTY()
	TArray<FVector2D> Uv0;
	UPROPERTY()
	TArray<FProcMeshTangent> Tangents;

	FSplineToolkitExtruderDrawData() = default;
	void Reserve(uint32 NumVertices);
	void Init(uint32 NumVertices);

	void Insert(const FSplineToolkitExtruderDrawData& Other, uint32 Where);

	int32 Num() const;
	bool IsEmpty() const;
};

USTRUCT()
struct FSplineToolkitExtruderMeshData
{
	GENERATED_BODY()

	// This is to see if it changed
	UPROPERTY()
	UStaticMesh* LinkedMesh = nullptr;
	UPROPERTY()
	FSplineToolkitExtruderDrawData OriginSlice;
	UPROPERTY()
	TArray<FSplineToolkitRmfSample> RmfSamples;
	UPROPERTY()
	TObjectPtr<AActor> MeshActor;
};

/**
 * This is an alternative to USplineMeshComponent that allows the use of SplineToolkit's modifiers and intersection rules
 */
UCLASS(ClassGroup=(Custom),
	meta=(BlueprintSpawnableComponent, ToolTip="A component that extrudes a given mesh along a given spline"),
	DisplayName="[Spline Toolkit] Mesh Extruder")
class SPLINETOOLKIT_API USplineToolkitMeshExtruder : public UActorComponent
{
	GENERATED_BODY()

public:

	virtual void OnRegister() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	UFUNCTION(CallInEditor, Category = "Spline Toolkit")
	void Regenerate();

	/** Clears all linked actors */
	UFUNCTION(CallInEditor, Category = "Spline Toolkit")
	void Clear();

	/** Clears out all data related to a rule but keeps old actors alive */
	void ClearConservative();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	bool bUpdateOnSplineChange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	USplineToolkitRuleset* Ruleset = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	USplineComponent* SplineComponent = nullptr;

private:

	// The output meshes
	UPROPERTY()
	TArray<FSplineToolkitExtruderMeshData> OutMeshes;

	void RecalculateRmfSamples(int32 NumRmfSamples, FSplineToolkitExtruderMeshData& Data) const;

	void ExtractOriginSlice(UStaticMesh* InputMesh, FSplineToolkitExtruderMeshData& Data) const;

	static TArray<int32> ComputeEndCap(const FSplineToolkitExtruderMeshData& Data, int16 IndexOffset, bool InvertOrdering);
	static TArray<int32> ReorderToLoop(const FRawStaticIndexBuffer& GeometryIndexBuffer, const TArray<FVector>& Positions, const TMap<int32, TArray<int32>>&
	                                   UsedIndices);

	void ComputeMesh(const FSplineToolkitExtrusionRule& Rule, UProceduralMeshComponent* MeshComponent, const FSplineToolkitExtruderMeshData& Data) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};
