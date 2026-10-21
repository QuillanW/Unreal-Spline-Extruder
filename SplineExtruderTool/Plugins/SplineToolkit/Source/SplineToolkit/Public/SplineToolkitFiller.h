// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer

#pragma once

#include "CoreMinimal.h"
#include "SplineToolkitIntersectionSolver.h"
#include "SplineToolkitRuleset.h"
#include "Components/ActorComponent.h"
#include "SplineToolkitFiller.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent, DisplayName = "[Spline Toolkit] Intersection Filler"))
class SPLINETOOLKIT_API USplineToolkitFiller : public UActorComponent
{
	GENERATED_BODY()

public:

	virtual void OnRegister() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(CallInEditor, BlueprintCallable)
	void Regenerate();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	bool bUpdateOnRulesetChange = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	bool bUpdateOnSplineChange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	USplineToolkitRuleset* Ruleset = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	USplineComponent* SplineComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Spline Toolkit")
	TArray<TObjectPtr<AActor>> IntersectionActors;

private:

	UPROPERTY()
	// Multiple indices (16-bit intersection index, 16-bit rule index)
	TMap<int32, TObjectPtr<AActor>> SpawnedActors;
	TMap<int32, TPair<FVector, FVector>> RuleTangents;
	int32 GetMapIndex(int32 IntersectionIndex, int32 RuleIndex);

	bool bRegenerate = false;

	TArray<FVector> DebugData;

	// Points where the meshes completely intersect
	// These points are then used as anhor points for connection rules
	struct FCollisionPoint
	{
		FVector Midpoint;
		float TotalWeight;

		USplineComponent* SplineA;
		ESplineToolkitRuleType RuleTypeA = ESplineToolkitRuleType::INSTANTIATION;
		uint8 RuleIndexA = 0;
		// This is just here because GetLocationClosestToWorldLocation does not work properly
		int32 PolyLineA;
		float DistanceA = 0.f;

		USplineComponent* SplineB;
		ESplineToolkitRuleType RuleTypeB = ESplineToolkitRuleType::INSTANTIATION;
		uint8 RuleIndexB = 0;
		// This is just here because GetLocationClosestToWorldLocation does not work properly
		int32 PolyLineB;
		float DistanceB = 0.f;
	};


	void RegenerateInternal(const USplineToolkitFiller* Caller = nullptr);

	static constexpr uint32 NumSamples = 32;
	using FPolyLine = TStaticArray<FVector, NumSamples>;

	FPolyLine CreatePolyLine(const FSplineToolkitSplineIntersection& Intersection, USplineComponent* Spline,
	                         const FSplineToolkitInstantiationRule& Rule);
	FPolyLine CreatePolyLine(const FSplineToolkitSplineIntersection& Intersection, USplineComponent* Spline,
	                         const FSplineToolkitExtrusionRule& Rule);

	TArray<FCollisionPoint> DetermineCollisionPoints(const FSplineToolkitSplineIntersection& Intersection);

	// Handles this edge case: User sets a rule to continue normally (from CUTOUT_START to CUTOUT_END on the same spline and rule)
	static bool RuleContinuesNormally(const FSplineToolkitFillingConnectRule& Rule);


	struct FConnectCtx
	{
		int32 IntersectionIndex;
		int32 IntersectionIndexB;
		const TArray<FCollisionPoint>* Collisions;
		FSplineToolkitSplineIntersection Intersection;
		const FSplineToolkitSplineIntersection* IntersectionPtr;
		FSplineToolkitSplineIntersection IntersectionB;
		int32 SplineIndex;
		const FSplineToolkitFillingRules* Rules;
		bool bFlip;
	};


	void RegenerateConnectRules(const FConnectCtx& Ctx);

	// Helper function to get data for one of the nodes of the connection rule
	using FSplineNodeData = TTuple<FVector, FVector, float, float>;
	TOptional<FSplineNodeData> GetPositionTangentForNode(const FConnectCtx& Ctx,
	                                                     const FSplineToolkitFillingConnectRule& Rule,
	                                                     const FSplineToolkitFillingConnectRuleParams& Params,
	                                                     const TOptional<FVector>& Check);

	void RegenerateConnectRule(const FConnectCtx& Ctx,
	                           int32 RuleIndex,
	                           const FSplineToolkitFillingConnectRule& Rule,
	                           const void* GeneratorRule,
	                           UClass* GeneratorClass);

	// A simple search algorithm to handle halfway tangent overrides
	void HandleHalfwayTangents(const FConnectCtx& Ctx);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};
