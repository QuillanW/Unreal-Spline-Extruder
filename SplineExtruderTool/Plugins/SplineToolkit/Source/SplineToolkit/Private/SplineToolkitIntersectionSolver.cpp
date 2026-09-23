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

int32 GSplineToolkitShowIntersectionCutoutRanges = 0;
static FAutoConsoleVariableRef CVarShowIntersectionCutoutRanges(
	TEXT("stk.IntersectionSolver.ShowCutoutRanges"),
	GSplineToolkitShowIntersectionCutoutRanges,
	TEXT(
		"Shows the cutout regions for spline overlaps"));


bool FSplineToolkitSplineIntersection::operator<(const FSplineToolkitSplineIntersection& Other) const
{
	return this->DistanceMin < Other.DistanceMin;
}


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

	if (GSplineToolkitShowIntersectionCutoutRanges)
	{
		for (const auto& Data : this->Collisions)
		{
			DrawDebugDirectionalArrow(GetWorld(), Data.Midpoint,
			                          this->SplineComponent->GetWorldLocationAtDistanceAlongSpline(
				                          Data.DistanceMin), 50.f, FColor::Cyan, false, -1, 0, 5.f);
			DrawDebugDirectionalArrow(GetWorld(), Data.Midpoint,
			                          this->SplineComponent->GetWorldLocationAtDistanceAlongSpline(
				                          Data.DistanceMax), 50.f, FColor::Cyan, false, -1, 0, 5.f);
		}
	}
}


// The intersection code is made with Claude Sonnet 5
// This because I could not be bothered and of course Unreal does not have
// a built-in way to do this in the editor reliably

static float ComputeApproxRadius(const FSplineToolkitExtrusionRule& Rule)
{
	if (!Rule.Mesh)
		return 0.f;

	const FBoxSphereBounds Bounds = Rule.Mesh->GetBounds();
	// Just calculate it with a spherical radius because I don't have time for this stuff
	return Bounds.SphereRadius * Rule.Scale.GetAbsMax();;
}


static FVector SampleOffsetLocation(USplineComponent* Spline, int32 Index, const FVector& Offset)
{
	const FVector Base = Spline->GetWorldLocationAtSplinePoint(Index);
	if (Offset.IsNearlyZero())
		return Base;
	const FQuat Frame = Spline->GetQuaternionAtSplinePoint(Index, ESplineCoordinateSpace::World);
	return Base + Frame.RotateVector(Offset);
}


static FVector SampleOffsetLocation(USplineComponent* Spline, float Distance, const FVector& Offset)
{
	const FVector Base = Spline->GetWorldLocationAtDistanceAlongSpline(Distance);
	if (Offset.IsNearlyZero())
		return Base;
	const FQuat Frame = Spline->GetQuaternionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	return Base + Frame.RotateVector(Offset);
}


TOptional<FSplineToolkitSplineIntersection> USplineToolkitIntersectionSolver::TestCollision(
	USplineComponent* SplineA,
	const FSplineToolkitExtrusionRule& RuleA,
	USplineComponent* SplineB,
	const FSplineToolkitExtrusionRule& RuleB,
	const float Tolerance)
{
	if (!SplineA || !SplineB || !RuleA.bCheckIntersections || !RuleB.bCheckIntersections)
		return NullOpt;

	const float RadiusA = ComputeApproxRadius(RuleA);
	const float RadiusB = ComputeApproxRadius(RuleB);
	const float CombinedRadius = (RadiusA + RadiusB) * (1.f - Tolerance);
	if (CombinedRadius <= 0.f)
		return NullOpt;
	const float CombinedRadiusSq = FMath::Square(CombinedRadius);

	// Simple prune of the entire spline
	FBox BoundsA{ForceInit};
	FBox BoundsB{ForceInit};

	auto CalcBounds = [&](USplineComponent* Comp, FBox& Box, const FSplineToolkitExtrusionRule& Rule)
	{
		for (int32 I = 0; I < Comp->GetNumberOfSplinePoints(); ++I)
			Box += SampleOffsetLocation(Comp, I, Rule.Offset);
		Box = Box.ExpandBy(CombinedRadius);
	};
	CalcBounds(SplineA, BoundsA, RuleA);
	CalcBounds(SplineB, BoundsB, RuleB);

	if (!FBoxSphereBounds::BoxesIntersect(BoundsA, BoundsB))
		return NullOpt;

	const int32 NumPointsA = SplineA->GetNumberOfSplinePoints();
	const int32 NumPointsB = SplineB->GetNumberOfSplinePoints();
	const int32 NumSegmentsA = SplineA->IsClosedLoop() ? NumPointsA : NumPointsA - 1;
	const int32 NumSegmentsB = SplineB->IsClosedLoop() ? NumPointsB : NumPointsB - 1;

	if (NumSegmentsA <= 0 || NumSegmentsB <= 0)
		return NullOpt;

	static constexpr int32 Samples = 128;
	static constexpr float SamplesF = static_cast<float>(Samples);

	// Collision check for one segment pair: subdivides both segments into polylines
	// and tests every sub-segment pair with the engine's segment/segment closest-point solver.
	auto CollideSegment = [&](int32 SegIndexA, int32 SegIndexB, TArray<TTuple<float, FVector, float>>& OutHits)
	{
		const float DistStartA = SplineA->GetDistanceAlongSplineAtSplinePoint(SegIndexA);
		const float DistEndA = SplineA->GetDistanceAlongSplineAtSplinePoint((SegIndexA + 1) % NumPointsA);
		const float DistStartB = SplineB->GetDistanceAlongSplineAtSplinePoint(SegIndexB);
		const float DistEndB = SplineB->GetDistanceAlongSplineAtSplinePoint((SegIndexB + 1) % NumPointsB);

		// Cheap segment-level reject before resampling either side at full density.
		FBox SegBoundsA(ForceInit);
		SegBoundsA += SampleOffsetLocation(SplineA, SegIndexA, RuleA.Offset);
		SegBoundsA += SampleOffsetLocation(SplineA, (SegIndexA + 1) % NumPointsA, RuleA.Offset);
		SegBoundsA = SegBoundsA.ExpandBy(CombinedRadius);

		FBox SegBoundsB(ForceInit);
		SegBoundsB += SampleOffsetLocation(SplineB, SegIndexB, RuleB.Offset);
		SegBoundsB += SampleOffsetLocation(SplineB, (SegIndexB + 1) % NumPointsB, RuleB.Offset);
		SegBoundsB = SegBoundsB.ExpandBy(CombinedRadius);

		if (!SegBoundsA.Intersect(SegBoundsB))
			return;

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

				const float DistSq = FVector::DistSquared(ClosestOnA, ClosestOnB);
				if (DistSq <= CombinedRadiusSq)
				{
					const float MidDistA = FMath::Lerp(DistA[i], DistA[i + 1], 0.5f);
					const FVector MidPoint = FMath::Lerp(ClosestOnA, ClosestOnB, 0.5f);
					OutHits.Emplace(MidDistA, MidPoint, DistSq);
				}
			}
		}
	};

	TArray<TTuple<float, FVector, float>> Hits;
	for (int32 A = 0; A < NumSegmentsA; ++A)
		for (int32 B = 0; B < NumSegmentsB; ++B)
			CollideSegment(A, B, Hits);

	if (Hits.IsEmpty())
		return NullOpt;

	float MinDist = TNumericLimits<float>::Max();
	float MaxDist = TNumericLimits<float>::Lowest();
	float BestSeparationSq = TNumericLimits<float>::Max();
	FVector Midpoint = FVector::ZeroVector;

	for (const auto& Hit : Hits)
	{
		const float DistAlongA = Hit.Get<0>();
		const float SepSq = Hit.Get<2>();

		MinDist = FMath::Min(MinDist, DistAlongA);
		MaxDist = FMath::Max(MaxDist, DistAlongA);

		if (SepSq < BestSeparationSq)
		{
			BestSeparationSq = SepSq;
			Midpoint = Hit.Get<1>();
		}
	}

	FSplineToolkitSplineIntersection Result{
		.DistanceMin = MinDist,
		.DistanceMax = MaxDist,
		.Midpoint = SplineA->FindLocationClosestToWorldLocation(Midpoint, ESplineCoordinateSpace::World)
	};
	return Result;
}


void USplineToolkitIntersectionSolver::SolveCollisionsFor(const USplineToolkitIntersectionSolver* Caller, const FSplineToolkitExtrusionRule& Rule)
{
	// Get the extruder
	auto* OwnerA = GetOwner();
	auto* ExtruderA = OwnerA->FindComponentByClass<USplineToolkitMeshExtruder>();
	auto* SplineA = OwnerA->FindComponentByClass<USplineComponent>();
	if (!ExtruderA || !SplineA)
		return;

	auto TestCollisionFor = [&](const AActor* OwnerB)
	{
		auto* ExtruderB = OwnerB->FindComponentByClass<USplineToolkitMeshExtruder>();
		auto* SplineB = OwnerB->FindComponentByClass<USplineComponent>();
		if (!ExtruderB || !SplineB || !IsValid(ExtruderB->Ruleset))
			return;

		for (const auto& RuleB : ExtruderB->Ruleset->ExtrusionRules)
		{
			if (!RuleB.bCheckIntersections)
				continue;

			auto Result = TestCollision(SplineA, Rule, SplineB, RuleB, this->Tolerance);

			if (!Result)
				continue;

			this->Collisions.Add(Result.GetValue());
		}
	};

	if (Caller)
	{
		// Only test against the caller to also have its collision
		auto* OwnerB = Caller->GetOwner();
		TestCollisionFor(OwnerB);
		return;
	}

	for (TObjectIterator<USplineToolkitMeshExtruder> It; It; ++It)
	{
		auto* Comp = *It;
		if (!IsValid(Comp) || Comp->GetWorld() != GetWorld() || Comp->GetOwner() == GetOwner())
			continue;

		auto* OwnerB = Comp->GetOwner();

		// Get the extruder
		TestCollisionFor(OwnerB);
	}
}


void USplineToolkitIntersectionSolver::SolveCollisions(const USplineToolkitIntersectionSolver* Caller)
{
	if (!IsValid(this->Ruleset))
		return;

	this->Collisions.Empty();
	for (const auto& Rule : this->Ruleset->ExtrusionRules)
	{
		if (Rule.bCheckIntersections)
			SolveCollisionsFor(Caller, Rule);
	}
	this->Collisions.Sort();
}
