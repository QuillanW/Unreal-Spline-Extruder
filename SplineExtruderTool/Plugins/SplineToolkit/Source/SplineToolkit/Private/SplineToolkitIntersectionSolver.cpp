// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer


#include "SplineToolkitIntersectionSolver.h"

#include "SplineToolkitInstantiator.h"
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

int32 GSplineToolkitShowIntersectionCheckBoxes = 0;
static FAutoConsoleVariableRef CVarShowIntersectionCheckBoxes(
	TEXT("stk.IntersectionSolver.ShowCheckBoxes"),
	GSplineToolkitShowIntersectionCheckBoxes,
	TEXT(
		"Shows all bounding boxes that are being checked when testing for spline intersection"));


bool FSplineToolkitSplineIntersection::operator<(const FSplineToolkitSplineIntersection& O) const
{
	return this->DistanceMin < O.DistanceMin;
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


static FVector SampleLeaveTangentOffsetLocation(USplineComponent* Spline, int32 Index, const FVector& Offset)
{
	const FVector Loc = Spline->GetWorldLocationAtSplinePoint(Index);
	if (Offset.IsNearlyZero())
		return Loc;
	const FQuat Frame = Spline->GetQuaternionAtSplinePoint(Index, ESplineCoordinateSpace::World);
	const FVector OffsetLoc = Loc + Frame.RotateVector(Offset);
	return OffsetLoc + 0.33f * Spline->GetLeaveTangentAtSplinePoint(Index, ESplineCoordinateSpace::World);
}


static FVector SampleArriveTangentOffsetLocation(USplineComponent* Spline, int32 Index, const FVector& Offset)
{
	const FVector Loc = Spline->GetWorldLocationAtSplinePoint(Index);
	if (Offset.IsNearlyZero())
		return Loc;
	const FQuat Frame = Spline->GetQuaternionAtSplinePoint(Index, ESplineCoordinateSpace::World);
	const FVector OffsetLoc = Loc + Frame.RotateVector(Offset);
	return OffsetLoc - 0.33f * Spline->GetArriveTangentAtSplinePoint(Index, ESplineCoordinateSpace::World);
}


static FVector SampleOffsetLocation(USplineToolkitRmfSampler* Sampler, float Distance, const FVector& Offset)
{
	auto Sample = Sampler->GetSampleAtDistance(Distance);
	Sample.Position += Sampler->GetOwner()->GetActorLocation();
	if (Offset.IsNearlyZero())
		return Sample.Position;
	const FQuat Frame = FMatrix{
		Sample.Bitangent.GetSafeNormal(),
		Sample.Tangent.GetSafeNormal(),
		Sample.Reference.GetSafeNormal(),
		FVector::ZeroVector
	}.ToQuat();
	return Sample.Position + Frame.RotateVector(Offset);
}


TOptional<TArray<FSplineToolkitSplineIntersection>> USplineToolkitIntersectionSolver::TestCollision(
	USplineComponent* SplineA,
	const FSplineToolkitExtrusionRule& RuleA,
	USplineComponent* SplineB,
	const FSplineToolkitExtrusionRule& RuleB)
{
	if (!SplineA || !SplineB || !RuleA.bCheckIntersections || !RuleB.bCheckIntersections)
		return NullOpt;

	auto* SamplerA = SplineA->GetOwner()->FindComponentByClass<USplineToolkitRmfSampler>();
	auto* SamplerB = SplineB->GetOwner()->FindComponentByClass<USplineToolkitRmfSampler>();

	if (!SamplerA || !SamplerB)
		return NullOpt;

	const float RadiusA = ComputeApproxRadius(RuleA);
	const float RadiusB = ComputeApproxRadius(RuleB);
	const float CombinedRadius = (RadiusA + RadiusB) * (1.f - Tolerance);
	if (CombinedRadius <= 0.f)
		return NullOpt;
	const float CombinedRadiusSq = FMath::Square(CombinedRadius);

	const int32 NumPointsA = SplineA->GetNumberOfSplinePoints();
	const int32 NumPointsB = SplineB->GetNumberOfSplinePoints();
	const int32 NumSegmentsA = SplineA->IsClosedLoop() ? NumPointsA : NumPointsA - 1;
	const int32 NumSegmentsB = SplineB->IsClosedLoop() ? NumPointsB : NumPointsB - 1;

	// Simple prune of the entire spline
	FBox BoundsA{ForceInit};
	FBox BoundsB{ForceInit};

	TArray<FBox> BoxesA{};
	TArray<FBox> BoxesB{};

	auto CalcBounds = [&](USplineComponent* Comp, FBox& SplineBox, TArray<FBox>& SegmentBoxes,
	                      const FSplineToolkitExtrusionRule& Rule)
	{
		const auto NumPointsComp = Comp->GetNumberOfSplinePoints();
		for (int32 I = 0; I < Comp->GetNumberOfSplinePoints() - 1; ++I)
		{
			FBox Box{ForceInit};
			Box += SampleOffsetLocation(Comp, I, Rule.Offset);
			Box += SampleOffsetLocation(Comp, (I + 1) % NumPointsComp, Rule.Offset);
			Box += SampleLeaveTangentOffsetLocation(Comp, I, Rule.Offset);
			Box += SampleArriveTangentOffsetLocation(Comp, (I + 1) % NumPointsComp, Rule.Offset);
			Box = Box.ExpandBy(CombinedRadius);
			SegmentBoxes.Add(Box);
			SplineBox += Box;
		}
	};
	CalcBounds(SplineA, BoundsA, BoxesA, RuleA);
	CalcBounds(SplineB, BoundsB, BoxesB, RuleB);

	auto* SolverB = SplineB->GetOwner()->FindComponentByClass<USplineToolkitIntersectionSolver>();

	if (GSplineToolkitShowIntersectionCheckBoxes)
	{
		DrawDebugBox(GetWorld(), BoundsA.GetCenter(), BoundsA.GetExtent(), FColor::Magenta);
		DrawDebugBox(GetWorld(), BoundsB.GetCenter(), BoundsB.GetExtent(), FColor::Magenta);
	}

	if (!FBoxSphereBounds::BoxesIntersect(BoundsA, BoundsB))
	{
		if (SolverB)
			SolverB->RemoveCollisionsWith(SplineA);
		return NullOpt;
	}

	if (NumSegmentsA <= 0 || NumSegmentsB <= 0)
		return NullOpt;

	static constexpr int32 Samples = 128;
	static constexpr float SamplesF = static_cast<float>(Samples);

	struct FIntersectInfo
	{
		int32 HitSegment;
		int32 HitSample;
		FVector2f Distances;
		FVector Midpoint;
		float DistanceSquared;
	};

	// Collision check for one segment pair: subdivides both segments into polylines
	// and tests every sub-segment pair with the engine's segment/segment closest-point solver.
	auto CollideSegment = [&](int32 SegIndexA, int32 SegIndexB, TArray<FIntersectInfo>& OutHits)
	{
		const float DistStartA = SplineA->GetDistanceAlongSplineAtSplinePoint(SegIndexA);
		const float DistEndA = SplineA->GetDistanceAlongSplineAtSplinePoint((SegIndexA + 1) % NumPointsA);
		const float DistStartB = SplineB->GetDistanceAlongSplineAtSplinePoint(SegIndexB);
		const float DistEndB = SplineB->GetDistanceAlongSplineAtSplinePoint((SegIndexB + 1) % NumPointsB);

		// Cheap segment-level reject before resampling either side at full density.
		const FBox& SegBoundsA = BoxesA[SegIndexA];
		const FBox& SegBoundsB = BoxesB[SegIndexB];

		if (GSplineToolkitShowIntersectionCheckBoxes)
		{
			DrawDebugBox(GetWorld(), SegBoundsA.GetCenter(), SegBoundsA.GetExtent(), FColor::Cyan);
			DrawDebugBox(GetWorld(), SegBoundsB.GetCenter(), SegBoundsB.GetExtent(), FColor::Cyan);
		}

		if (!SegBoundsA.Intersect(SegBoundsB))
		{
			if (SolverB)
				SolverB->RemoveCollisionsWith(SplineA, SegIndexA);
			return;
		}

		TArray<FVector, TInlineAllocator<33>> PolyA, PolyB;
		TArray<float, TInlineAllocator<33>> DistA;

		for (int32 i = 0; i <= Samples; ++i)
		{
			const float D = FMath::Lerp(DistStartA, DistEndA, i / SamplesF);
			DistA.Add(D);
			PolyA.Add(SampleOffsetLocation(SamplerA, D, RuleA.Offset));
		}
		for (int32 j = 0; j <= Samples; ++j)
		{
			const float D = FMath::Lerp(DistStartB, DistEndB, j / SamplesF);
			PolyB.Add(SampleOffsetLocation(SamplerB, D, RuleB.Offset));
		}

		bool bFoundOne = false;
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
					bFoundOne = true;
					const FVector MidPoint = FMath::Lerp(ClosestOnA, ClosestOnB, 0.5f);
					OutHits.Add(FIntersectInfo{
						.HitSegment = SegIndexB,
						.HitSample = i,
						.Distances = FVector2f{DistA[i], DistA[i + 1]},
						.Midpoint = MidPoint,
						.DistanceSquared = DistSq
					});
				}
			}
		}
		if (!bFoundOne)
		{
			if (SolverB)
				SolverB->RemoveCollisionsWith(SplineA, SegIndexA);
		}
	};

	TArray<FIntersectInfo> Hits;
	for (int32 A = 0; A < NumSegmentsA; ++A)
		for (int32 B = 0; B < NumSegmentsB; ++B)
			CollideSegment(A, B, Hits);

	if (Hits.IsEmpty())
		return NullOpt;

	Hits.Sort([](const FIntersectInfo& A, const FIntersectInfo& B)
	{
		if (A.HitSegment < B.HitSegment)
			return true;
		return A.HitSample < B.HitSample;
	});

	TArray<FSplineToolkitSplineIntersection> Result;

	// Island search for multiple intersections within a segment
	int32 LastAdded = -1;
	for (const auto& Hit : Hits)
	{
		if (LastAdded == -1 || Hit.HitSample - LastAdded > 1)
		{
			auto& Intersection = Result.Emplace_GetRef();
			Intersection.DistanceMin = Hit.Distances.X;
			Intersection.Other = SplineB;
			Intersection.OtherSegment = Hit.HitSegment;
		}

		auto& Intersection = Result.Last();

		const float Sigma2 = CombinedRadiusSq * 0.25f;
		const float Weight = FMath::Exp(-Hit.DistanceSquared / (2.f * Sigma2));
		Intersection.Midpoint += Weight * Hit.Midpoint;
		Intersection.TotalWeight += Weight;
		Intersection.DistanceMax = Hit.Distances.Y;
		LastAdded = Hit.HitSample;
	}

	for (auto& Intersection : Result)
	{
		Intersection.Midpoint /= Intersection.TotalWeight;
		// Calculate the angle
		FVector TangentA = this->SplineComponent->FindTangentClosestToWorldLocation(
			Intersection.Midpoint, ESplineCoordinateSpace::World).GetSafeNormal();
		FVector TangentB = Intersection.Other->FindTangentClosestToWorldLocation(
			Intersection.Midpoint, ESplineCoordinateSpace::World).GetSafeNormal();
		Intersection.AbsoluteAngleDifference = FMath::Abs(FMath::RadiansToDegrees(FMath::Acos(TangentA.Dot(TangentB))));
	}

	return Result;
}


void USplineToolkitIntersectionSolver::SolveCollisionsFor(const USplineToolkitIntersectionSolver* Caller,
                                                          const FSplineToolkitExtrusionRule& Rule)
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
		auto* SamplerB = OwnerB->FindComponentByClass<USplineToolkitRmfSampler>();
		auto* SplineB = OwnerB->FindComponentByClass<USplineComponent>();
		if (!ExtruderB || !SplineB || !IsValid(ExtruderB->Ruleset))
			return;

		for (const auto& RuleB : ExtruderB->Ruleset->ExtrusionRules)
		{
			if (!RuleB.bCheckIntersections)
				continue;

			auto Result = TestCollision(SplineA, Rule, SplineB, RuleB);

			if (!Result)
				continue;

			if (SamplerB && SamplerB->Samples.IsEmpty())
				SamplerB->Regenerate();

			ExtruderB->MarkDirty();

			if (auto* InstantiatorB = OwnerB->FindComponentByClass<USplineToolkitInstantiator>())
				InstantiatorB->MarkDirty();

			if (auto* IntersectionB = OwnerB->FindComponentByClass<USplineToolkitIntersectionSolver>();
				IntersectionB && !Caller)
				IntersectionB->SolveCollisions(this);

			this->Collisions.Append(Result.GetValue());
		}
	};

	if (Caller)
	{
		// Only test against the caller to also have its collision
		auto* OwnerB = Caller->GetOwner();
		TestCollisionFor(OwnerB);
		return;
	}

	for (TObjectIterator<USplineToolkitRmfSampler> It; It; ++It)
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

	// First remove degenerate ones
	RemoveDegenerate();

	if (Caller)
		this->Collisions.RemoveAll([&](const FSplineToolkitSplineIntersection& Intersection)
		{
			return Intersection.Other == Caller->SplineComponent;
		});
	else
		this->Collisions.Empty();

	for (const auto& Rule : this->Ruleset->ExtrusionRules)
	{
		if (Rule.bCheckIntersections)
			SolveCollisionsFor(Caller, Rule);
	}
	this->Collisions.Sort();
}


void USplineToolkitIntersectionSolver::RemoveDegenerate()
{
	this->Collisions.RemoveAll([](const FSplineToolkitSplineIntersection& Intersection)
	{
		return Intersection.Other == nullptr;
	});
}


void USplineToolkitIntersectionSolver::RemoveCollisionsWith(USplineComponent* Spline, int32 Segment)
{
	int32 NumRemoved = 0;
	if (Segment == INT32_MAX)
	{
		NumRemoved = this->Collisions.RemoveAll([&](const FSplineToolkitSplineIntersection& Intersection)
		{
			return Intersection.Other == Spline;
		});
	}
	else
	{
		NumRemoved = this->Collisions.RemoveAll([&](const FSplineToolkitSplineIntersection& Intersection)
		{
			return Intersection.Other == Spline && Intersection.OtherSegment == Segment;
		});
	}

	if (NumRemoved == 0)
		return;

	if (auto* Extruder = this->SplineComponent->GetOwner()->FindComponentByClass<USplineToolkitMeshExtruder>())
		Extruder->MarkDirty();
	if (auto* Instantiator = this->SplineComponent->GetOwner()->FindComponentByClass<USplineToolkitInstantiator>())
		Instantiator->MarkDirty();
}
