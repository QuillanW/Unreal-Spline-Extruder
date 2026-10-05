// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer


#include "SplineToolkitFiller.h"

#include "SplineToolkitInstantiator.h"
#include "SplineToolkitIntersectionSolver.h"
#include "SplineToolkitMeshExtruder.h"
#include "SplineToolkitModifier.h"

int32 GSplineToolkitShowFillIntersections = 0;
static FAutoConsoleVariableRef CVarShowFillIntersections(
	TEXT("stk.IntersectionFiller.ShowIntersections"),
	GSplineToolkitShowFillIntersections,
	TEXT(
		"Shows where rules overlap on spline intersections"));

int32 GSplineToolkitShowFillPolylines = 0;
static FAutoConsoleVariableRef CVarShowPolylines(
	TEXT("stk.IntersectionFiller.ShowPolylines"),
	GSplineToolkitShowFillPolylines,
	TEXT(
		"Shows the polylines that the filler checks against. (Only when dragging the spline)"));


void USplineToolkitFiller::OnRegister()
{
	Super::OnRegister();

	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;

	if (const AActor* Owner = GetOwner())
	{
		if (!Owner->FindComponentByClass<USplineComponent>())
		{
			UE_LOG(LogTemp, Error, TEXT("Extruder requires USplineComponent"));
			return;
		}
		this->SplineComponent = Owner->GetComponentByClass<USplineComponent>();
		this->SplineComponent->GetOnSplineChanged().AddLambda([this]()
		{
			if (this->bUpdateOnSplineChange)
				Regenerate();
		});
	}

	if (this->Ruleset)
	{
		this->Ruleset->OnShouldRegenerate.AddLambda([this]
		{
			if (this->bUpdateOnRulesetChange)
				Regenerate();
		});
	}
}


void USplineToolkitFiller::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (this->bRegenerate)
	{
		RegenerateInternal();
		this->bRegenerate = false;
	}

	if (GSplineToolkitShowFillIntersections)
	{
		for (const auto& Midpoint : this->TempDebugData)
			DrawDebugSphere(GetWorld(), Midpoint, 10.f, 16, FColor::White);
	}
}


void USplineToolkitFiller::Regenerate()
{
	bRegenerate = true;
}


void USplineToolkitFiller::RegenerateInternal()
{
	if (!this->Ruleset)
		return;

	auto* IntersectionSolver = GetOwner()->FindComponentByClass<USplineToolkitIntersectionSolver>();
	if (!IsValid(IntersectionSolver))
		return;

	this->TempDebugData.Empty();

	for (const auto& Intersection : IntersectionSolver->Collisions)
	{
		auto Collision = DetermineCollisionPoints(Intersection);
		for (const auto& Point : Collision)
			this->TempDebugData.Add(Point.Midpoint);

		for (const auto& Rules : this->Ruleset->FillingRules)
		{
			if (!FMath::IsWithinInclusive(Intersection.AbsoluteAngleDifference, Rules.AngleMin, Rules.AngleMax))
				continue;
			RegenerateConnectRules(Rules);
			break;
		}
	}
}


static FVector SampleOffsetLocationFill(USplineToolkitRmfSampler* Sampler, float Distance, const FVector& Offset)
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


USplineToolkitFiller::FPolyLine USplineToolkitFiller::CreatePolyLine(
	const FSplineToolkitSplineIntersection& Intersection, USplineComponent* Spline, const
	FSplineToolkitInstantiationRule& Rule)
{
	auto* Sampler = Spline->GetOwner()->FindComponentByClass<USplineToolkitRmfSampler>();
	if (!Sampler)
		return {};
	FPolyLine Result{};
	for (int32 I = 0; I < NumSamples; ++I)
	{
		float Distance = FMath::Lerp(Intersection.DistanceMin, Intersection.DistanceMax,
		                             static_cast<float>(I) / static_cast<float>(NumSamples - 1));

		FSplineToolkitStepContext Context{Spline, Distance};

		auto ModdedRule = Rule;
		for (const auto& Modifier : Rule.Modifiers)
			ModdedRule = Modifier->ModifyInstantiationStep(Context, ModdedRule);

		Result[I] = SampleOffsetLocationFill(Sampler, Distance, ModdedRule.Offset);
	}
	return Result;
}


USplineToolkitFiller::FPolyLine USplineToolkitFiller::CreatePolyLine(
	const FSplineToolkitSplineIntersection& Intersection, USplineComponent* Spline,
	const FSplineToolkitExtrusionRule& Rule)
{
	auto* Sampler = Spline->GetOwner()->FindComponentByClass<USplineToolkitRmfSampler>();
	if (!Sampler)
		return {};
	FPolyLine Result{};
	for (int32 I = 0; I < NumSamples; ++I)
	{
		float Distance = FMath::Lerp(Intersection.DistanceMin, Intersection.DistanceMax,
		                             static_cast<float>(I) / static_cast<float>(NumSamples - 1));

		FSplineToolkitStepContext Context{Spline, Distance};

		auto ModdedRule = Rule;
		for (const auto& Modifier : Rule.Modifiers)
			ModdedRule = Modifier->ModifyExtrusionStep(Context, ModdedRule);

		Result[I] = SampleOffsetLocationFill(Sampler, Distance, ModdedRule.Offset);
	}
	return Result;
}


TArray<USplineToolkitFiller::FCollisionPoint> USplineToolkitFiller::DetermineCollisionPoints(
	const FSplineToolkitSplineIntersection& Intersection)
{
	// The approach:
	// - Make polylines for all rules on this spline and the one that it collides with
	// - Intersect all those lines
	// - Store the points where they intersect

	auto* Solver = GetOwner()->FindComponentByClass<USplineToolkitIntersectionSolver>();
	auto* SolverB = Intersection.Other->GetOwner()->FindComponentByClass<USplineToolkitIntersectionSolver>();
	if (!Solver || !SolverB)
		return {};

	auto* IntersectionB = SolverB->Collisions.FindByPredicate(
		[&](const FSplineToolkitSplineIntersection& TestIntersection)
		{
			return TestIntersection.Other == this->SplineComponent && (TestIntersection.Midpoint - Intersection.
				Midpoint).SquaredLength() < 100.f;
		});

	if (!IntersectionB)
		return {};

	TArray<FCollisionPoint> Result{};

	struct FPolyLineInfo
	{
		FPolyLine Line;

		USplineComponent* Spline;
		ESplineToolkitRuleType RuleType = ESplineToolkitRuleType::INSTANTIATION;
		uint8 RuleIndex = 0;
	};

	auto CollidePolylines = [&](const TArray<FPolyLineInfo>& PolyLines)
	{
		for (int32 I = 0; I < PolyLines.Num(); ++I)
		{
			for (int32 J = 0; J < PolyLines.Num(); ++J)
			{
				if (I == J)
					continue;

				const auto& PolyA = PolyLines[I];
				const auto& PolyB = PolyLines[J];

				struct FHitInfo
				{
					FVector Midpoint;
					float DistanceSquared;
					int32 HitSample;
				};
				TArray<FHitInfo> OutHits{};

				// Take samples on both lines to
				for (int32 SampleA = 0; SampleA < NumSamples - 1; ++SampleA)
				{
					// Debug view
					if (GSplineToolkitShowFillPolylines)
						DrawDebugLine(GetWorld(), PolyA.Line[SampleA], PolyA.Line[SampleA + 1], FColor::Cyan);

					for (int32 SampleB = 0; SampleB < NumSamples - 1; ++SampleB)
					{
						FVector ClosestOnA, ClosestOnB;
						FMath::SegmentDistToSegmentSafe(
							PolyA.Line[SampleA], PolyA.Line[SampleA + 1],
							PolyB.Line[SampleB], PolyB.Line[SampleB + 1],
							ClosestOnA, ClosestOnB);

						const float DistSq = FVector::DistSquared(ClosestOnA, ClosestOnB);
						// Hardcoded because I cannot be bothered
						static constexpr float Cutoff = 1000.f;
						if (DistSq <= Cutoff)
						{
							const FVector MidPoint = FMath::Lerp(ClosestOnA, ClosestOnB, 0.5f);
							OutHits.Add(FHitInfo{
								.Midpoint = MidPoint,
								.DistanceSquared = DistSq,
								.HitSample = SampleA,
							});
						}
					}
				}

				if (OutHits.IsEmpty())
					continue;

				OutHits.Sort([](const FHitInfo& A, const FHitInfo& B)
				{
					return A.HitSample < B.HitSample;
				});

				int32 LastAdded = -1;
				for (const auto& Hit : OutHits)
				{
					if (LastAdded == -1 || Hit.HitSample - LastAdded > 1)
					{
						auto& Collision = Result.Emplace_GetRef();
						Collision.SplineA = PolyA.Spline;
						Collision.RuleTypeA = PolyA.RuleType;
						Collision.RuleIndexA = PolyA.RuleIndex;
						Collision.SplineB = PolyB.Spline;
						Collision.RuleTypeB = PolyB.RuleType;
						Collision.RuleIndexB = PolyB.RuleIndex;
					}

					auto& Collision = Result.Last();

					const float Sigma2 = 25.f;
					const float Weight = FMath::Exp(-Hit.DistanceSquared / (2.f * Sigma2));
					Collision.Midpoint += Weight * Hit.Midpoint;
					Collision.TotalWeight += Weight;
					LastAdded = Hit.HitSample;
				}
			}
		}
	};

	TArray<FPolyLineInfo> PolyLines{};

	// Helper function to collect all polylines for all rules in a ruleset
	auto LoopRules = [&](const FSplineToolkitSplineIntersection& LocalIntersection, USplineComponent* Spline)
	{
		// Get the ruleset from either component that is present
		USplineToolkitRuleset* Ruleset = nullptr;
		if (auto* Extruder = Spline->GetOwner()->FindComponentByClass<USplineToolkitMeshExtruder>())
			Ruleset = Extruder->Ruleset;
		else if (auto* Instantiator = Spline->GetOwner()->FindComponentByClass<USplineToolkitInstantiator>())
			Ruleset = Instantiator->Ruleset;

		if (!Ruleset)
			return;

		uint8 RuleIndex = 0;
		for (const auto& Rule : Ruleset->InstantiationRules)
		{
			PolyLines.Emplace(CreatePolyLine(LocalIntersection, Spline, Rule), Spline,
			                  ESplineToolkitRuleType::INSTANTIATION, RuleIndex++);
		}
		RuleIndex = 0;
		for (const auto& Rule : Ruleset->ExtrusionRules)
		{
			PolyLines.Emplace(CreatePolyLine(LocalIntersection, Spline, Rule), Spline,
			                  ESplineToolkitRuleType::EXTRUSION, RuleIndex++);
		}
	};

	LoopRules(Intersection, this->SplineComponent);
	LoopRules(*IntersectionB, Intersection.Other);

	CollidePolylines(PolyLines);

	for (auto& Collision : Result)
		Collision.Midpoint /= Collision.TotalWeight;

	return Result;
}


bool USplineToolkitFiller::RuleContinuesNormally(const FSplineToolkitFillingConnectRule& Rule)
{
	return Rule.Start.SplineIndex == Rule.End.SplineIndex
		&& Rule.Start.RuleType == Rule.End.RuleType
		&& Rule.Start.RuleIndex == Rule.End.RuleIndex
		&& Rule.Start.OffsetDistance == 0.f
		&& Rule.End.OffsetDistance == 0.f
		&& Rule.Start.Type == EFillAnchorType::CUTOUT_START
		&& Rule.End.Type == EFillAnchorType::CUTOUT_END
		&& Rule.RuleIndex == Rule.Start.RuleIndex
		&& Rule.RuleType == Rule.Start.RuleType
		&& Rule.SplineIndex == Rule.Start.SplineIndex;
}


void USplineToolkitFiller::RegenerateConnectRules(const FSplineToolkitFillingRules& Rules)
{
	for (const auto& ConnectRule : Rules.ConnectRules)
	{
		switch (ConnectRule.RuleType)
		{
		case ESplineToolkitRuleType::INSTANTIATION:
		{
			// const auto& Rule = this->Ruleset->InstantiationRules[ConnectRule.RuleIndex];
			RegenerateConnectRule(ConnectRule, USplineToolkitInstantiator::StaticClass());
			break;
		}
		case ESplineToolkitRuleType::EXTRUSION:
		{
			// const auto& Rule = this->Ruleset->ExtrusionRules[ConnectRule.RuleIndex];
			RegenerateConnectRule(ConnectRule, USplineToolkitMeshExtruder::StaticClass());
			break;
		}
		case ESplineToolkitRuleType::STRETCH:
		{
			// const auto& Rule = this->Ruleset->StretchRules[ConnectRule.RuleIndex];
			RegenerateConnectRule(ConnectRule, USplineToolkitMeshExtruder::StaticClass());
			break;
		}
		}
	}
}


void USplineToolkitFiller::RegenerateConnectRule(const FSplineToolkitFillingConnectRule& Rule, UClass* GeneratorClass)
{
	if (RuleContinuesNormally(Rule))
	{
		auto* Comp = GetOwner()->FindComponentByClass(GeneratorClass);

		if (GeneratorClass == USplineToolkitInstantiator::StaticClass())
			Cast<USplineToolkitInstantiator>(Comp)->bIgnoreIntersectCutouts = true;
		else if (GeneratorClass == USplineToolkitMeshExtruder::StaticClass())
			Cast<USplineToolkitMeshExtruder>(Comp)->bIgnoreIntersectCutouts = true;

		return;
	}
}


void USplineToolkitFiller::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (this->Ruleset->IsValidLowLevel())
	{
		this->Ruleset->OnShouldRegenerate.AddLambda([this]
		{
			if (this->bUpdateOnRulesetChange)
				Regenerate();
		});
	}
}
