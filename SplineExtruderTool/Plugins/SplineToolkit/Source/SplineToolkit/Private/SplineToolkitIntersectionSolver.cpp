// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer


#include "SplineToolkitIntersectionSolver.h"

#include "ProceduralMeshComponent.h"
#include "SplineToolkitMeshExtruder.h"

int32 GSplineToolkitShowIntersectionMidpoints = 0;
static FAutoConsoleVariableRef CVarShowIntersectionMidpoints(
	TEXT("stk.IntersectionSolver.ShowMidpoints"),
	GSplineToolkitShowIntersectionMidpoints,
	TEXT(
		"Shows the midpoints for spline overlaps"));


void USplineToolkitIntersectionSolver::OnRegister()
{
	Super::OnRegister();

	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;

	if (const AActor* Owner = GetOwner())
	{
		if (!Owner->FindComponentByClass<USplineComponent>())
		{
			UE_LOG(LogTemp, Error, TEXT("Instantiator requires USplineComponent"));
			return;
		}
		this->SplineComponent = Owner->GetComponentByClass<USplineComponent>();
		this->SplineComponent->GetOnSplineChanged().AddLambda([this]()
		{
			SolveCollisions();
		});
	}
}


void USplineToolkitIntersectionSolver::TickComponent(float DeltaTime, ELevelTick TickType,
                                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GSplineToolkitShowIntersectionMidpoints)
	{
		for (const auto& Data : this->Collisions)
			DrawDebugSphere(GetWorld(), Data.Midpoint, 50.f, 16, FColor::White);
	}
}


// TOptional<FVector> USplineToolkitIntersectionSolver::TestCollision(UProceduralMeshComponent* A,
// 	UProceduralMeshComponent* B)
// {
// 	if (!A || !B)
// 		return NullOpt;
//
// 	FBox BoxA = A->Bounds.GetBox();
// 	FBox BoxB = B->Bounds.GetBox();
// 	if (!BoxA.Intersect(BoxB))
// 		return NullOpt;
//
// 	FComponentQueryParams Params;
// 	Params.bTraceComplex = true;
// 	Params.AddIgnoredComponent(A);
//
// 	// Sweep B along a short offset through its own current pose so the sweep actually registers a hit
// 	// rather than starting already-overlapping (which sweeps report as a zero-distance/blocking start hit, not a surface point)
// 	FVector Offset = (BoxA.GetCenter() - BoxB.GetCenter()).GetSafeNormal() * 1.0f;
// 	FVector StartLoc = B->GetComponentLocation() - Offset * 50.f;
// 	FVector EndLoc = B->GetComponentLocation();
//
// 	TArray<FHitResult> Hits;
// 	bool bHit = GetWorld()->ComponentSweepMulti(Hits, B, StartLoc, EndLoc, B->GetComponentQuat(), Params);
//
// 	if (!bHit)
// 		return NullOpt;
//
// 	for (const FHitResult& Hit : Hits)
// 	{
// 		if (Hit.GetActor() == A->GetOwner() || Hit.Component == A)
// 			return Hit.ImpactPoint;
// 	}
//
// 	return NullOpt;
// }


static float ComputeApproxRadius(const FSplineToolkitExtrusionRule& Rule)
{
	if (!Rule.Mesh)
		return 0.f;

	const FBoxSphereBounds Bounds = Rule.Mesh->GetBounds();
	// Just calculate it with a spherical radius because I don't have time for this stuff
	return Bounds.SphereRadius * Rule.Scale.GetAbsMax();;
}

static FVector SampleOffsetLocation(USplineComponent* Spline, float Distance, const FVector& Offset)
{
	const FVector Base = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	if (Offset.IsNearlyZero())
		return Base;
	const FQuat Frame = Spline->GetQuaternionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	return Base + Frame.RotateVector(Offset);
}


TOptional<USplineToolkitIntersectionSolver::FSplineIntersection> USplineToolkitIntersectionSolver::TestCollision(
	USplineComponent* SplineA,
	const FSplineToolkitExtrusionRule& RuleA,
	USplineComponent* SplineB,
	const FSplineToolkitExtrusionRule& RuleB)
{
	if (!SplineA || !SplineB || !RuleA.bCheckIntersections || !RuleB.bCheckIntersections)
		return NullOpt;

	const float RadiusA = ComputeApproxRadius(RuleA);
	const float RadiusB = ComputeApproxRadius(RuleB);
	const float CombinedRadius = RadiusA + RadiusB;
	if (CombinedRadius <= 0.f)
		return NullOpt;
	const float CombinedRadiusSq = FMath::Square(CombinedRadius);

	const int32 NumPointsA = SplineA->GetNumberOfSplinePoints();
	const int32 NumPointsB = SplineB->GetNumberOfSplinePoints();
	const int32 NumSegmentsA = SplineA->IsClosedLoop() ? NumPointsA : NumPointsA - 1;
	const int32 NumSegmentsB = SplineB->IsClosedLoop() ? NumPointsB : NumPointsB - 1;

	if (NumSegmentsA <= 0 || NumSegmentsB <= 0)
		return NullOpt;

	static constexpr int32 Samples = 64;
	static constexpr float SamplesF = 64.f;

	// Collision check for one segment pair: subdivides both segments into polylines
	// (density taken from each rule's RMF sample count) and tests every sub-segment
	// pair with the engine's segment/segment closest-point solver.
	auto CollideSegment = [&](int32 SegIndexA, int32 SegIndexB, TArray<TPair<float, FVector>>& OutHits)
    {
        const float DistStartA = SplineA->GetDistanceAlongSplineAtSplinePoint(SegIndexA);
        const float DistEndA   = SplineA->GetDistanceAlongSplineAtSplinePoint((SegIndexA + 1) % NumPointsA);
        const float DistStartB = SplineB->GetDistanceAlongSplineAtSplinePoint(SegIndexB);
        const float DistEndB   = SplineB->GetDistanceAlongSplineAtSplinePoint((SegIndexB + 1) % NumPointsB);

        TArray<FVector, TInlineAllocator<33>> PolyA, PolyB;
        TArray<float, TInlineAllocator<33>> DistA;

        for (int32 i = 0; i <= Samples; ++i)
        {
            const float D = FMath::Lerp(DistStartA, DistEndA, i / SamplesF);
            DistA.Add(D);
            PolyA.Add(SampleOffsetLocation(SplineA, D, RuleA.Offset));
        }
        for (int32 j = 0; j <= Samples; ++j)
        {
            const float D = FMath::Lerp(DistStartB, DistEndB, j / SamplesF);
            PolyB.Add(SampleOffsetLocation(SplineB, D, RuleB.Offset));
        }

        for (int32 i = 0; i < Samples; ++i)
        {
            for (int32 j = 0; j < Samples; ++j)
            {
                FVector ClosestOnA, ClosestOnB;
                FMath::SegmentDistToSegmentSafe(
                    PolyA[i], PolyA[i + 1],
                    PolyB[j], PolyB[j + 1],
                    ClosestOnA, ClosestOnB);

                if (FVector::DistSquared(ClosestOnA, ClosestOnB) <= CombinedRadiusSq)
                {
                    const float MidDistA = FMath::Lerp(DistA[i], DistA[i + 1], 0.5f);
                    const FVector MidPoint = FMath::Lerp(ClosestOnA, ClosestOnB, 0.5f);
                    OutHits.Emplace(MidDistA, MidPoint);
                }
            }
        }
    };

    TArray<TPair<float, FVector>> Hits;
    for (int32 a = 0; a < NumSegmentsA; ++a)
        for (int32 b = 0; b < NumSegmentsB; ++b)
            CollideSegment(a, b, Hits);

    if (Hits.IsEmpty())
    	return NullOpt;

    float MinDist = TNumericLimits<float>::Max();
    float MaxDist = TNumericLimits<float>::Lowest();
    FVector MidpointAtMin = FVector::ZeroVector;
    FVector MidpointAtMax = FVector::ZeroVector;

    for (const TPair<float, FVector>& Hit : Hits)
    {
        if (Hit.Key < MinDist)
        {
            MinDist = Hit.Key;
            MidpointAtMin = Hit.Value;
        }
        if (Hit.Key > MaxDist)
        {
            MaxDist = Hit.Key;
            MidpointAtMax = Hit.Value;
        }
    }

    FSplineIntersection Result;
    Result.Range = FFloatRange::Inclusive(MinDist, MaxDist);
    Result.Midpoint = FMath::Lerp(MidpointAtMin, MidpointAtMax, 0.5f);
    return Result;
}


void USplineToolkitIntersectionSolver::SolveCollisionsFor(const FSplineToolkitExtrusionRule& Rule)
{
	// Get the extruder
	auto* OwnerA = GetOwner();
	auto* ExtruderA = OwnerA->FindComponentByClass<USplineToolkitMeshExtruder>();
	auto* SplineA = OwnerA->FindComponentByClass<USplineComponent>();
	if (!ExtruderA || !SplineA)
		return;

	for (TObjectIterator<USplineToolkitMeshExtruder> It; It; ++It)
	{
		auto* Comp = *It;
		if (!IsValid(Comp) || Comp->GetWorld() != GetWorld() || Comp->GetOwner() == GetOwner())
			continue;

		auto* OwnerB = Comp->GetOwner();

		// Get the extruder
		auto* ExtruderB = OwnerB->FindComponentByClass<USplineToolkitMeshExtruder>();
		auto* SplineB = OwnerB->FindComponentByClass<USplineComponent>();
		if (!ExtruderB || !SplineB || !IsValid(ExtruderB->Ruleset))
			continue;

		for (const auto& RuleB : ExtruderB->Ruleset->ExtrusionRules)
		{
			if (!RuleB.bCheckIntersections)
				continue;

			auto Result = TestCollision(SplineA, Rule, SplineB, RuleB);

			if (!Result)
				continue;

			this->Collisions.Add(Result.GetValue());
		}
	}
}


void USplineToolkitIntersectionSolver::SolveCollisions()
{
	if (!IsValid(this->Ruleset))
		return;

	this->Collisions.Empty();
	for (const auto& Rule : this->Ruleset->ExtrusionRules)
	{
		if (Rule.bCheckIntersections)
			SolveCollisionsFor(Rule);
	}
}
