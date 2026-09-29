// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "SplineToolkitRmf.h"
#include "SplineToolkitIntersectionSolver.h"
#include "SplineToolkitRuleset.h"
#include "Components/SplineComponent.h"
#include "Runtime/Engine/Classes/Components/StaticMeshComponent.h"
#include "SplineToolkitMeshExtruder.generated.h"


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
	UPROPERTY()
	TArray<int32> Indices;

	FSplineToolkitExtruderDrawData() = default;
	void ReserveVertices(uint32 NumVertices);
	void InitVertices(uint32 NumVertices);

	void ReserveIndices(uint32 NumIndices);
	void InitIndices(uint32 NumIndices);

	void InsertVertices(const FSplineToolkitExtruderDrawData& Other, uint32 Where);
	void AppendIndices(TArray<int32>&& List);
	void AddIndex(int32 Index);

	void ShrinkFit();

	int32 VertexTop() const;
	int32 IndexTop() const;

	int32 VertexNum() const;
	bool IsEmpty() const;

private:

	UPROPERTY()
	int32 VertexTopIdx = 0;
	UPROPERTY()
	int32 IndexTopIdx = 0;
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
	TObjectPtr<AActor> MeshActor;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ProceduralMeshComponent;
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

	AActor* GetAssociatedActorOfRule(const FSplineToolkitExtrusionRule& Rule) const;

	UFUNCTION(CallInEditor, Category = "Spline Toolkit")
	void Regenerate();

	/** Clears all linked actors */
	UFUNCTION(CallInEditor, Category = "Spline Toolkit")
	void Clear();

	/** Clears out all data related to a rule but keeps old actors alive */
	void ClearConservative();

	void MarkDirty();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	bool bUpdateOnRulesetChange = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	bool bUpdateOnSplineChange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	USplineToolkitRuleset* Ruleset = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	USplineComponent* SplineComponent = nullptr;

private:

	void RegenerateInternal();
	void ReapplyMaterials();

	bool bRegenerate = false;

	// The output meshes
	UPROPERTY()
	TArray<FSplineToolkitExtruderMeshData> OutMeshes;

	void ExtractOriginSlice(UStaticMesh* InputMesh, FSplineToolkitExtruderMeshData& Data) const;
	
	USplineToolkitRulesetModifierBase* GetOrCreateModifierInstance(TSubclassOf<USplineToolkitRulesetModifierBase> Class);

	static TArray<int32> ComputeEndCap(const FSplineToolkitExtruderMeshData& Data, int32 IndexOffset,
	                                   bool InvertOrdering);
	static TArray<int32> ReorderToLoop(const FRawStaticIndexBuffer& GeometryIndexBuffer,
	                                   const TArray<FVector>& Positions, const TMap<int32, TArray<int32>>&
	                                   UsedIndices);

	void AddStartCap(USplineToolkitIntersectionSolver* Solver, FSplineToolkitExtruderDrawData& DrawData,
	                 const FSplineToolkitExtrusionRule& Rule, const FSplineToolkitExtruderMeshData& Data) const;
	void AddEndCap(USplineToolkitIntersectionSolver* Solver, FSplineToolkitExtruderDrawData& DrawData,
	               const FSplineToolkitExtrusionRule& Rule, const FSplineToolkitExtruderMeshData& Data) const;

	/// Appends a new instance of the slice to the draw data
	/// Returns a couple of things of data:
	///		The RMF sample to transform the vertices
	///		The index in the RMF array
	///		The index into the vertex list
	///	The return value indices that no new sample was added (end of spline)
	bool AddNextSampleToMesh(USplineToolkitIntersectionSolver* Solver,
	                         FSplineToolkitExtruderDrawData& DrawData, const FSplineToolkitExtrusionRule& Rule,
	                         const FSplineToolkitExtruderMeshData& Data, FSplineToolkitRmfSample& OutRmfSample,
	                         int32& OutVertexPtr, bool& OutDontConnect, bool bCalledFromSelf = false) const;

	void ConnectToPreviousSample(FSplineToolkitExtruderDrawData& DrawData,
	                             int32 StartIndex, const FSplineToolkitExtruderMeshData& Data) const;

	void ComputeMesh(const FSplineToolkitExtrusionRule& Rule, UProceduralMeshComponent* MeshComponent,
	                 const FSplineToolkitExtruderMeshData& Data) const;

	UPROPERTY(Transient)
	TMap<TSubclassOf<USplineToolkitRulesetModifierBase>, USplineToolkitRulesetModifierBase*> ModifierInstanceCache;
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};
