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

	bool bRegenerate = false;

	TArray<FVector> TempDebugData;

	// Points where the meshes completely intersect
	// These points are then used as anhor points for connection rules
	struct FCollisionPoint
	{
		FVector Midpoint;
		float TotalWeight;

		USplineComponent* SplineA;
		ESplineToolkitRuleType RuleTypeA = ESplineToolkitRuleType::INSTANTIATION;
		uint8 RuleIndexA = 0;

		USplineComponent* SplineB;
		ESplineToolkitRuleType RuleTypeB = ESplineToolkitRuleType::INSTANTIATION;
		uint8 RuleIndexB = 0;
	};


	void RegenerateInternal();

	static constexpr uint32 NumSamples = 32;
	using FPolyLine = TStaticArray<FVector, NumSamples>;

	FPolyLine CreatePolyLine(const FSplineToolkitSplineIntersection& Intersection, USplineComponent* Spline,
	                         const FSplineToolkitInstantiationRule& Rule);
	FPolyLine CreatePolyLine(const FSplineToolkitSplineIntersection& Intersection, USplineComponent* Spline,
	                         const FSplineToolkitExtrusionRule& Rule);

	TArray<FCollisionPoint> DetermineCollisionPoints(const FSplineToolkitSplineIntersection& Intersection);

	// Handles this edge case: User sets a rule to continue normally (from CUTOUT_START to CUTOUT_END on the same spline and rule)
	static bool RuleContinuesNormally(const FSplineToolkitFillingConnectRule& Rule);

	void RegenerateConnectRules(const FSplineToolkitFillingRules& Rules);

	void RegenerateConnectRule(const FSplineToolkitFillingConnectRule& Rule,
	                           UClass* GeneratorClass);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};
