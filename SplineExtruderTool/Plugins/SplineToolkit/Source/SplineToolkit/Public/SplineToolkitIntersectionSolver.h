// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer

#pragma once

#include "CoreMinimal.h"
#include "SplineToolkitRuleset.h"
#include "Components/ActorComponent.h"
#include "Components/SplineComponent.h"
#include "SplineToolkitIntersectionSolver.generated.h"


USTRUCT(BlueprintType)
struct FSplineToolkitSplineIntersection
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	float DistanceMin;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	float DistanceMax;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	FVector Midpoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	USplineComponent* Other;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	int32 OtherSegment;

	int32 SamplesIncluded = 0;

	bool operator<(const FSplineToolkitSplineIntersection& O) const;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), DisplayName="[Spline Toolkit] Intersection Solver")
class SPLINETOOLKIT_API USplineToolkitIntersectionSolver : public UActorComponent
{
	GENERATED_BODY()

protected:

	// Called when the game starts
	virtual void OnRegister() override;

public:

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(CallInEditor, BlueprintCallable)
	void SolveCollisions(const USplineToolkitIntersectionSolver* Caller = nullptr);

	void RemoveDegenerate();
	void RemoveCollisionsWith(USplineComponent* Spline, int32 Segment = INT32_MAX);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	USplineToolkitRuleset* Ruleset = nullptr;

	// Should be [0, 1]
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	float Tolerance = 0.1f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	USplineComponent* SplineComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	TArray<FSplineToolkitSplineIntersection> Collisions;

private:

	// Returns the range where a spline mesh intersects another
	TOptional<TArray<FSplineToolkitSplineIntersection>> TestCollision(USplineComponent* SplineA,
	                                                                  const FSplineToolkitExtrusionRule& RuleA,
	                                                                  USplineComponent* SplineB,
	                                                                  const FSplineToolkitExtrusionRule& RuleB);

	void SolveCollisionsFor(const USplineToolkitIntersectionSolver* Caller, const FSplineToolkitExtrusionRule& Rule);

};
