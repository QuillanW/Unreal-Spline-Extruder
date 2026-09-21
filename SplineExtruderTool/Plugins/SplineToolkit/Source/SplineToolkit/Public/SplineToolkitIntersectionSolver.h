// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "SplineToolkitRuleset.h"
#include "Components/ActorComponent.h"
#include "Components/SplineComponent.h"
#include "SplineToolkitIntersectionSolver.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
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


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	USplineToolkitRuleset* Ruleset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	float Precision = 0.9f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	USplineComponent* SplineComponent = nullptr;

private:

	struct FSplineIntersection
	{
		FFloatRange Range;
		FVector Midpoint;
	};


	// Returns the range where a spline mesh intersects another
	static TOptional<FSplineIntersection> TestCollision(USplineComponent* SplineA, const FSplineToolkitExtrusionRule& RuleA,
	                                                    USplineComponent* SplineB, const FSplineToolkitExtrusionRule& RuleB);

	void SolveCollisionsFor(const FSplineToolkitExtrusionRule& Rule);
	void SolveCollisions();

	TArray<FSplineIntersection> Collisions;

};
