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
		for (const auto& Midpoint : this->DebugData)
			DrawDebugSphere(GetWorld(), Midpoint, 10.f, 16, FColor::White);
	}
}


void USplineToolkitFiller::Regenerate()
{
	bRegenerate = true;
}


int32 USplineToolkitFiller::GetMapIndex(int32 IntersectionIndex, int32 RuleIndex)
{
	return ((int16)IntersectionIndex << 16) | (int16)RuleIndex;
}


void USplineToolkitFiller::RegenerateInternal(const USplineToolkitFiller* Caller)
{
	if (!this->Ruleset)
		return;

	auto* IntersectionSolver = GetOwner()->FindComponentByClass<USplineToolkitIntersectionSolver>();
	if (!IsValid(IntersectionSolver))
		return;

	this->DebugData.Empty();

	if (auto* Extruder = GetOwner()->FindComponentByClass<USplineToolkitMeshExtruder>())
		Extruder->IgnoreIntersectCutouts.Empty();
	if (auto* Instantiator = GetOwner()->FindComponentByClass<USplineToolkitInstantiator>())
		Instantiator->IgnoreIntersectCutouts.Empty();

	if (IntersectionSolver->Collisions.IsEmpty())
	{
		for (auto& [_, Actor] : this->SpawnedActors)
			Actor->Destroy();
		this->SpawnedActors.Empty();
		return;
	}

	int32 IntersectionIndex = 0;
	for (int32 I = 0; I < IntersectionSolver->Collisions.Num(); ++I, ++IntersectionIndex)
	{
		const auto& Intersection = IntersectionSolver->Collisions[I];
		auto Collision = DetermineCollisionPoints(Intersection);
		if (Collision.IsEmpty())
			continue;
		for (const auto& Point : Collision)
			this->DebugData.Add(Point.Midpoint);

		// Store the other intersection for reference
		auto* SolverB = Intersection.Other->GetOwner()->FindComponentByClass<USplineToolkitIntersectionSolver>();
		if (!SolverB)
			continue;

		int32 IntersectionIndexB = SolverB->Collisions.IndexOfByPredicate(
			[&](const FSplineToolkitSplineIntersection& TestIntersection)
			{
				return TestIntersection.Other == this->SplineComponent && (TestIntersection.Midpoint - Intersection.
					Midpoint).SquaredLength() < 100.f;
			});
		if (IntersectionIndexB == INDEX_NONE)
			continue;

		const int32 SplineIndex = GetOwner()->GetUniqueID() < Intersection.Other->GetOwner()->GetUniqueID() ? 0 : 1;

		// Flip intersection range when the tangents are pointing opposite
		auto IntersectionB = SolverB->Collisions[IntersectionIndexB];
		auto* SplineA = SplineIndex == 0 ? this->SplineComponent : Intersection.Other;
		auto* SplineB = SplineIndex == 1 ? this->SplineComponent : Intersection.Other;
		auto TangentA = SplineA->
			FindTangentClosestToWorldLocation(Intersection.Midpoint, ESplineCoordinateSpace::World);
		auto TangentB = SplineB->
			FindTangentClosestToWorldLocation(Intersection.Midpoint, ESplineCoordinateSpace::World);
		bool bFlip = TangentA.Dot(TangentB) < 0.f;

		FConnectCtx Ctx{
			.IntersectionIndex = IntersectionIndex,
			.IntersectionIndexB = IntersectionIndexB,
			.Collisions = &Collision,
			.Intersection = Intersection,
			.IntersectionPtr = &Intersection,
			.IntersectionB = IntersectionB,
			.SplineIndex = SplineIndex,
			.bFlip = bFlip
		};

		for (const auto& Rules : this->Ruleset->FillingRules)
		{
			if (!FMath::IsWithinInclusive(Intersection.AngleDifference, Rules.AngleMin, Rules.AngleMax))
				continue;
			Ctx.Rules = &Rules;
			RegenerateConnectRules(Ctx);
			break;
		}
		if (auto* Filler = Intersection.Other->GetOwner()->FindComponentByClass<USplineToolkitFiller>();
			Filler && !Caller)
			Filler->RegenerateInternal(this);
		HandleHalfwayTangents(Ctx);
	}

	// Remove unused actors
	TArray<int32> ToRemove{};
	for (const auto& [Index, Actor] : this->SpawnedActors)
	{
		if ((BitCast<int32>(BitCast<uint32>(Index) & 0xffff0000) >> 16) >= IntersectionIndex)
		{
			Actor->Destroy();
			ToRemove.Add(Index);
		}
	}
	for (int32 Index : ToRemove)
		this->SpawnedActors.Remove(Index);
}


#pragma region Collision Points

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

		FSplineToolkitStepContext Context{Spline, Sampler->GetSampleAtDistance(Distance)};

		auto ModdedRule = Rule;
		for (const auto& Modifier : Rule.Modifiers)
		{
			if (Modifier->IsValidLowLevel())
				ModdedRule = Modifier->ModifyInstantiationStep(Context, ModdedRule);
		}

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
		FSplineToolkitStepContext Context{Spline, Sampler->GetSampleAtDistance(Distance)};

		auto ModdedRule = Rule;
		for (const auto& Modifier : Rule.Modifiers)
		{
			if (Modifier->IsValidLowLevel())
				ModdedRule = Modifier->ModifyExtrusionStep(Context, ModdedRule);
		}

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
		// I know about FFloatRange, but the API on that is SUPER inconvenient to use
		FVector2f DistanceRange;
	};

	auto CollidePolylines = [&](const TArray<FPolyLineInfo>& PolyLines)
	{
		for (int32 I = 0; I < PolyLines.Num(); ++I)
		{
			for (int32 J = I + 1; J < PolyLines.Num(); ++J)
			{
				const auto& PolyA = PolyLines[I];
				const auto& PolyB = PolyLines[J];
				if (PolyA.Spline == PolyB.Spline)
					continue;

				struct FHitInfo
				{
					FVector Midpoint;
					float DistanceSquared;
					int32 PolyLineA;
					float HitSampleA;
					int32 PolyLineB;
					float HitSampleB;
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

							auto SegFraction = [](const FVector& P, const FVector& S0, const FVector& S1) -> float
							{
								const FVector D = S1 - S0;
								const float LenSq = D.SizeSquared();
								return LenSq > KINDA_SMALL_NUMBER
									       ? FMath::Clamp(FVector::DotProduct(P - S0, D) / LenSq, 0.f, 1.f)
									       : 0.f;
							};

							OutHits.Add(FHitInfo{
								.Midpoint = MidPoint,
								.DistanceSquared = DistSq,
								.PolyLineA = I,
								.HitSampleA = SampleA + SegFraction(ClosestOnA, PolyA.Line[SampleA],
								                                    PolyA.Line[SampleA + 1]),
								.PolyLineB = J,
								.HitSampleB = SampleB + SegFraction(ClosestOnB, PolyB.Line[SampleB],
								                                    PolyB.Line[SampleB + 1]),
							});
						}
					}
				}

				if (OutHits.IsEmpty())
					continue;

				OutHits.Sort([](const FHitInfo& A, const FHitInfo& B)
				{
					return A.HitSampleA < B.HitSampleA;
				});

				int32 LastAdded = -1;
				for (const auto& Hit : OutHits)
				{
					if (LastAdded == -1 || Hit.HitSampleA - LastAdded > 1)
					{
						auto& Collision = Result.Emplace_GetRef();
						Collision.SplineA = PolyA.Spline;
						Collision.RuleTypeA = PolyA.RuleType;
						Collision.RuleIndexA = PolyA.RuleIndex;
						Collision.SplineB = PolyB.Spline;
						Collision.RuleTypeB = PolyB.RuleType;
						Collision.RuleIndexB = PolyB.RuleIndex;
						Collision.PolyLineA = I;
						Collision.PolyLineB = J;
					}

					auto& Collision = Result.Last();

					static constexpr float Sigma2 = 25.f;
					const float Weight = FMath::Exp(-Hit.DistanceSquared / (2.f * Sigma2));
					Collision.Midpoint += Weight * Hit.Midpoint;
					Collision.TotalWeight += Weight;
					Collision.DistanceA += Weight * Hit.HitSampleA;
					Collision.DistanceB += Weight * Hit.HitSampleB;
					LastAdded = Hit.HitSampleA;
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
			PolyLines.Emplace(CreatePolyLine(LocalIntersection, Spline, Rule),
			                  Spline,
			                  ESplineToolkitRuleType::INSTANTIATION,
			                  RuleIndex++,
			                  FVector2f{LocalIntersection.DistanceMin, LocalIntersection.DistanceMax}
			);
		}
		RuleIndex = 0;
		for (const auto& Rule : Ruleset->ExtrusionRules)
		{
			PolyLines.Emplace(CreatePolyLine(LocalIntersection, Spline, Rule),
			                  Spline,
			                  ESplineToolkitRuleType::EXTRUSION,
			                  RuleIndex++,
			                  FVector2f{LocalIntersection.DistanceMin, LocalIntersection.DistanceMax}
			);
		}
	};

	LoopRules(Intersection, this->SplineComponent);
	LoopRules(*IntersectionB, Intersection.Other);

	CollidePolylines(PolyLines);

	for (auto& Collision : Result)
	{
		Collision.Midpoint /= Collision.TotalWeight;

		Collision.DistanceA /= Collision.TotalWeight;
		const auto& DistancesA = PolyLines[Collision.PolyLineA].DistanceRange;
		Collision.DistanceA = FMath::Lerp(DistancesA.X, DistancesA.Y,
		                                  Collision.DistanceA / static_cast<float>(NumSamples - 1));

		Collision.DistanceB /= Collision.TotalWeight;
		const auto& DistancesB = PolyLines[Collision.PolyLineB].DistanceRange;
		Collision.DistanceB = FMath::Lerp(DistancesB.X, DistancesB.Y,
		                                  Collision.DistanceB / static_cast<float>(NumSamples - 1));
	}

	return Result;
}

#pragma endregion


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


void USplineToolkitFiller::RegenerateConnectRules(const FConnectCtx& Ctx)
{
	int32 RuleIndex = 0;
	for (const auto& ConnectRule : Ctx.Rules->ConnectRules)
	{
		if (ConnectRule.SplineIndex != Ctx.SplineIndex)
		{
			++RuleIndex;
			continue;
		}

		switch (ConnectRule.RuleType)
		{
		case ESplineToolkitRuleType::INSTANTIATION:
		{
			const auto& Rule = this->Ruleset->InstantiationRules[ConnectRule.RuleIndex];
			RegenerateConnectRule(Ctx, RuleIndex, ConnectRule, &Rule, USplineToolkitInstantiator::StaticClass());
			break;
		}
		case ESplineToolkitRuleType::EXTRUSION:
		{
			const auto& Rule = this->Ruleset->ExtrusionRules[ConnectRule.RuleIndex];
			RegenerateConnectRule(Ctx, RuleIndex, ConnectRule, &Rule, USplineToolkitMeshExtruder::StaticClass());
			break;
		}
		case ESplineToolkitRuleType::STRETCH:
		{
			const auto& Rule = this->Ruleset->StretchRules[ConnectRule.RuleIndex];
			RegenerateConnectRule(Ctx, RuleIndex, ConnectRule, &Rule, USplineToolkitMeshExtruder::StaticClass());
			break;
		}
		}

		++RuleIndex;
	}
}


TOptional<USplineToolkitFiller::FSplineNodeData> USplineToolkitFiller::GetPositionTangentForNode(
	const FConnectCtx& Ctx, const FSplineToolkitFillingConnectRule& Rule,
	const FSplineToolkitFillingConnectRuleParams& Params, const TOptional<FVector>& Check)
{
	const FSplineToolkitSplineIntersection& Intersect = Params.SplineIndex == Ctx.SplineIndex
		                                                    ? Ctx.Intersection
		                                                    : Ctx.IntersectionB;
	USplineComponent* Spline = Rule.SplineIndex == Ctx.SplineIndex ? this->SplineComponent : Ctx.Intersection.Other;

	auto* Sampler = Spline->GetOwner()->FindComponentByClass<USplineToolkitRmfSampler>();
	if (!Sampler)
		return FSplineNodeData{};

	switch (Params.Type)
	{
	case EFillAnchorType::CUTOUT_START:
	{
		Spline = Params.SplineIndex == Ctx.SplineIndex ? this->SplineComponent : Ctx.Intersection.Other;
		const bool bFlipp = Ctx.bFlip && Params.SplineIndex != Ctx.SplineIndex;
		const float Distance = bFlipp
			                       ? Intersect.DistanceMax - Params.OffsetDistance
			                       : Intersect.DistanceMin + Params.OffsetDistance;
		FVector Tangent = Spline->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
		if (Ctx.bFlip)
			Tangent = -Tangent;
		return FSplineNodeData{
			Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
			Tangent,
			Distance,
			0.f // Only used when intersecting a rule
		};
	}
	case EFillAnchorType::CUTOUT_END:
	{
		Spline = Params.SplineIndex == Ctx.SplineIndex ? this->SplineComponent : Ctx.Intersection.Other;
		const bool bFlipp = Ctx.bFlip && Params.SplineIndex != Ctx.SplineIndex;
		const float Distance = bFlipp
			                       ? Intersect.DistanceMin + Params.OffsetDistance
			                       : Intersect.DistanceMax - Params.OffsetDistance;
		FVector Tangent = Spline->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
		if (Ctx.bFlip)
			Tangent = -Tangent;
		return FSplineNodeData{
			Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
			Spline->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
			Distance,
			0.f // Only used when intersecting a rule
		};
	}
	case EFillAnchorType::INTERSECT_RULE:
	{
		TArray<TPair<float, const FCollisionPoint*>> Candidates{};

		for (const auto& Collision : *Ctx.Collisions)
		{
			const bool bIsA = Collision.SplineA == Spline;
			const bool bIsB = Collision.SplineB == Spline;
			if (!bIsA && !bIsB)
				continue;

			// Rule on the side of the spline we're anchoring to
			const ESplineToolkitRuleType ThisType = bIsA ? Collision.RuleTypeA : Collision.RuleTypeB;
			const int32 ThisIndex = bIsA ? Collision.RuleIndexA : Collision.RuleIndexB;
			const ESplineToolkitRuleType CheckType = bIsB ? Collision.RuleTypeA : Collision.RuleTypeB;
			const int32 CheckIndex = bIsB ? Collision.RuleIndexA : Collision.RuleIndexB;
			if (ThisType != Rule.RuleType || ThisIndex != Rule.RuleIndex
				|| CheckType != Params.RuleType || CheckIndex != Params.RuleIndex)
				continue;

			const float Dist = Spline->GetDistanceAlongSplineAtLocation(
				Collision.Midpoint, ESplineCoordinateSpace::World);

			const float CheckDist = Check
				                        ? Spline->GetDistanceAlongSplineAtLocation(
					                        Check.GetValue(), ESplineCoordinateSpace::World)
				                        : (Ctx.bFlip ? Intersect.DistanceMax : Intersect.DistanceMin);

			const float Score = FMath::Abs(Dist - CheckDist);
			Candidates.Emplace(Score, &Collision);
		}
		if (Candidates.IsEmpty())
			return NullOpt;

		Candidates.Sort();

		float Distance = 0.f;
		float DistanceB = 0.f;
		const auto* Candidate = Candidates[FMath::Clamp(Params.IntersectionIndex, 0ui8, Candidates.Num() - 1ui8)].Value;
		const bool bIsA = Candidate->SplineA == Spline;
		const bool bIsB = Candidate->SplineB == Spline;
		if (bIsA || bIsB)
		{
			Distance = bIsA ? Candidate->DistanceA : Candidate->DistanceB;
			DistanceB = bIsB ? Candidate->DistanceA : Candidate->DistanceB;
		}
		Distance += Params.OffsetDistance;
		return FSplineNodeData{
			Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
			Spline->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
			Distance,
			DistanceB
		};
	}
	}
	return NullOpt;
}


void USplineToolkitFiller::RegenerateConnectRule(const FConnectCtx& Ctx, int32 RuleIndex,
                                                 const FSplineToolkitFillingConnectRule& Rule,
                                                 const void* GeneratorRule, UClass* GeneratorClass)
{
	if (Rule.Start == Rule.End)
		return;

	if (RuleContinuesNormally(Rule))
	{
		auto* Comp = GetOwner()->FindComponentByClass(GeneratorClass);

		if (GeneratorClass == USplineToolkitInstantiator::StaticClass())
			Cast<USplineToolkitInstantiator>(Comp)->IgnoreIntersectCutouts.Emplace(Rule.RuleIndex, Ctx.IntersectionPtr);
		else if (GeneratorClass == USplineToolkitMeshExtruder::StaticClass())
			Cast<USplineToolkitMeshExtruder>(Comp)->IgnoreIntersectCutouts.Emplace(Rule.RuleIndex, Ctx.IntersectionPtr);

		return;
	}

	TObjectPtr<AActor> Actor = nullptr;\
	int32 MapIndex = GetMapIndex(Ctx.IntersectionIndex, RuleIndex);
	if (auto* Found = SpawnedActors.Find(MapIndex); Found && IsValid(*Found))
	{
		Actor = this->SpawnedActors[MapIndex];
	}
	else
	{
		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Actor = GetWorld()->SpawnActor<AActor>(Params);
#if WITH_EDITOR
		Actor->SetActorLabel(FString::Printf(TEXT("FillingRule%d"), RuleIndex));
#endif
		this->SpawnedActors.Emplace(MapIndex, Actor);
	}

	auto& Tangents = this->RuleTangents.FindOrAdd(MapIndex);

	// Me having to make this lambda is saying something about the engine
	auto AddComponent = [this, &Actor]<typename TComponent>(const bool bMakeRoot = false) -> TComponent*
	{
		TComponent* Comp = Actor->FindComponentByClass<TComponent>();
		if (!Comp)
		{
			Comp = NewObject<TComponent>(Actor, NAME_None, RF_Transactional);
			Comp->CreationMethod = EComponentCreationMethod::Instance;
			if constexpr (std::is_base_of_v<USceneComponent, TComponent>)
				Comp->SetupAttachment(Actor->GetRootComponent());
			Comp->RegisterComponent();
			Actor->AddInstanceComponent(Comp);
			if constexpr (std::is_base_of_v<USceneComponent, TComponent>)
			{
				if (bMakeRoot)
				{
					Actor->SetRootComponent(Comp);
					Actor->AttachToActor(GetOwner(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				}
			}
		}
		return Comp;
	};

	// Add a spline component
	USplineComponent* SplineComp = AddComponent.operator()<USplineComponent>(true);

	// Helper function
	auto ResultStart = GetPositionTangentForNode(Ctx, Rule, Rule.Start, NullOpt);

	if (!ResultStart)
	{
		Actor->Destroy();
		return;
	}

	auto [SplineStart, SplineLeaveTangent, DistanceStart, DistanceStartB] = *ResultStart;

	auto ResultEnd = GetPositionTangentForNode(Ctx, Rule, Rule.End, SplineStart);

	if (!ResultEnd)
	{
		Actor->Destroy();
		return;
	}

	auto [SplineEnd, SplineArriveTangent, DistanceEnd, DistanceEndB] = *ResultEnd;

	FVector CheckVector = SplineEnd - SplineStart;

	if (CheckVector.Dot(SplineArriveTangent) > 0.f)
		SplineArriveTangent *= -1.f;
	if (CheckVector.Dot(SplineLeaveTangent) < 0.f)
		SplineLeaveTangent *= -1.f;

	bool bOnSameSpline = Rule.Start.SplineIndex == Rule.End.SplineIndex || Rule.Start.Type ==
		EFillAnchorType::INTERSECT_RULE || Rule.End.Type == EFillAnchorType::INTERSECT_RULE;
	if (!bOnSameSpline)
	{
		SplineArriveTangent = SplineStart - SplineEnd;
		SplineLeaveTangent = SplineEnd - SplineStart;
	}
	else
	{
		// Scale the tangent based on time difference
		float TimeDifference = FMath::Abs(
			this->SplineComponent->GetTimeAtDistanceAlongSpline(DistanceEnd) - this->SplineComponent->
			GetTimeAtDistanceAlongSpline(DistanceStart));
		SplineLeaveTangent *= TimeDifference;
		SplineArriveTangent *= -TimeDifference;
	}

	Tangents.Key = SplineLeaveTangent;
	Tangents.Value = SplineArriveTangent;

	if (SplineComp->GetNumberOfSplinePoints() == 0)
	{
		SplineComp->AddSplinePoint(SplineStart, ESplineCoordinateSpace::World);
		SplineComp->AddSplinePoint(SplineEnd, ESplineCoordinateSpace::World);
	}
	else
	{
		SplineComp->SetWorldLocationAtSplinePoint(0, SplineStart);
		SplineComp->SetWorldLocationAtSplinePoint(1, SplineEnd);
	}

	SplineComp->SetTangentAtSplinePoint(0, SplineLeaveTangent, ESplineCoordinateSpace::World);
	SplineComp->SetTangentAtSplinePoint(1, SplineArriveTangent, ESplineCoordinateSpace::World);

	// Initialise the RMF sampler
	auto* Sampler = AddComponent.operator()<USplineToolkitRmfSampler>();
	Sampler->NumRmfSamples = Rule.NumSamples;

	// Add the generators
	if (GeneratorClass == USplineToolkitInstantiator::StaticClass())
	{
		auto* Instantiator = AddComponent.operator()<USplineToolkitInstantiator>();
		Instantiator->Overrides.ModifierSpline = this->SplineComponent;
		Instantiator->Overrides.Rule = *static_cast<const FSplineToolkitInstantiationRule*>(GeneratorRule);
		Instantiator->AutoUpdate = true;
		Instantiator->Overrides.RuleIndex = RuleIndex;
		Instantiator->Overrides.Distance = bOnSameSpline ? TOptional{FVector2f{DistanceStart, DistanceEnd}} : NullOpt;
	}
	else if (GeneratorClass == USplineToolkitMeshExtruder::StaticClass())
	{
		auto* Extruder = AddComponent.operator()<USplineToolkitMeshExtruder>();
		Extruder->Overrides.ModifierSpline = this->SplineComponent;
		Extruder->Overrides.Rule = *static_cast<const FSplineToolkitExtrusionRule*>(GeneratorRule);
		Extruder->bUpdateOnSplineChange = true;
		Extruder->Overrides.RuleIndex = RuleIndex;
		Extruder->Overrides.Distance = bOnSameSpline ? TOptional{FVector2f{DistanceStart, DistanceEnd}} : NullOpt;

		// Tangent overrides
		USplineComponent* SplineB = Rule.SplineIndex == Ctx.SplineIndex
			                            ? Ctx.Intersection.Other
			                            : this->SplineComponent;
		auto GetTangentOverride = [&](const FSplineToolkitFillingConnectRuleParams& Params) -> TOptional<FVector>
		{
			if (Params.Type != EFillAnchorType::INTERSECT_RULE)
				return NullOpt;

			bool bIsStart = Params == Rule.Start;

			const float DistB = bIsStart ? DistanceStartB : DistanceEndB;

			const FVector TangentB = -SplineB->GetTangentAtDistanceAlongSpline(DistB, ESplineCoordinateSpace::World).
			                                   GetSafeNormal();
			const FVector PosA = bIsStart ? SplineStart : SplineEnd;
			const FVector PosB = SplineB->GetLocationAtDistanceAlongSpline(DistB, ESplineCoordinateSpace::World);

			FVector NormalB = FVector::UpVector.Cross(TangentB).GetSafeNormal();
			if ((NormalB | (PosB - PosA)) < 0.f)
				NormalB = -NormalB;

			switch (Params.TangentDirection)
			{
			case EConnectionTangentDirection::FACE_OTHER_SPLINE:
				return NormalB;
			default:
				return NullOpt;
			}
		};

		Extruder->Overrides.StartTangent = GetTangentOverride(Rule.Start);
		Extruder->Overrides.EndTangent = GetTangentOverride(Rule.End);
	}
}


static bool ShouldCheckHalfway(const FSplineToolkitFillingConnectRuleParams& Params)
{
	return Params.Type == EFillAnchorType::INTERSECT_RULE && FMath::IsNearlyZero(Params.OffsetDistance) && Params.
		RuleType == ESplineToolkitRuleType::EXTRUSION;
}


void USplineToolkitFiller::HandleHalfwayTangents(const FConnectCtx& Ctx)
{
	auto* Other = Ctx.Intersection.Other->GetOwner()->GetComponentByClass<USplineToolkitFiller>();

	if (!Other)
		return;

	for (int32 I = 0; I < Ctx.Rules->ConnectRules.Num(); ++I)
	{
		const auto& Rule = Ctx.Rules->ConnectRules[I];

		int32 MapIndex = GetMapIndex(Ctx.IntersectionIndex, I);
		if (!this->SpawnedActors.Contains(MapIndex))
			continue;

		const auto& Actor = this->SpawnedActors[MapIndex];

		if (!IsValid(Actor))
			continue;

		auto* Extruder = Actor->GetComponentByClass<USplineToolkitMeshExtruder>();

		if (!Extruder || !IsValid(Extruder))
			continue;

		const auto& Tangents = this->RuleTangents[MapIndex];

		auto CheckNode = [&](const FSplineToolkitFillingConnectRuleParams& Params)
		{
			if (!ShouldCheckHalfway(Params))
				return;

			bool bIsStart = Params == Rule.Start;
			const FVector Tangent = (bIsStart ? Tangents.Key : Tangents.Value).GetSafeNormal();
			TOptional<FVector>* Override = bIsStart
				                               ? &Extruder->Overrides.StartTangent
				                               : &Extruder->Overrides.EndTangent;

			for (int32 J = I + 1; J < Ctx.Rules->ConnectRules.Num(); ++J)
			{
				const auto& RuleB = Ctx.Rules->ConnectRules[J];

				int32 MapIndexB = GetMapIndex(Ctx.IntersectionIndex, J);
				int32 MapIndexBSplineB = GetMapIndex(Ctx.IntersectionIndexB, J);
				const TObjectPtr<AActor>* ActorB = nullptr;
				const TPair<FVector, FVector>* TangentsB = nullptr;

				if (this->SpawnedActors.Contains(MapIndexB))
				{
					ActorB = &this->SpawnedActors[MapIndexB];
					if (this->RuleTangents.Contains(MapIndexB))
						TangentsB = &this->RuleTangents[MapIndexB];
				}
				else if (Other->SpawnedActors.Contains(MapIndexBSplineB))
				{
					ActorB = &Other->SpawnedActors[MapIndexBSplineB];
					if (Other->RuleTangents.Contains(MapIndexBSplineB))
						TangentsB = &Other->RuleTangents[MapIndexBSplineB];
				}

				if (!ActorB || !TangentsB || !IsValid(*ActorB))
					continue;

				auto* ExtruderB = (*ActorB)->GetComponentByClass<USplineToolkitMeshExtruder>();

				auto CheckNodeB = [&](const FSplineToolkitFillingConnectRuleParams& ParamsB)
				{
					if (!ShouldCheckHalfway(ParamsB))
						return false;

					const bool bSwapped =
						ParamsB.SplineIndex == Rule.SplineIndex && ParamsB.RuleType == Rule.RuleType && ParamsB.
						RuleIndex == Rule.RuleIndex &&
						Params.SplineIndex == RuleB.SplineIndex && Params.RuleType == RuleB.RuleType && Params.RuleIndex
						== RuleB.RuleIndex;
					if (!bSwapped)
						return false;

					bool bIsStartB = ParamsB == RuleB.Start;
					FVector TangentB = (bIsStartB ? TangentsB->Key : TangentsB->Value).GetSafeNormal();
					if (bIsStart == bIsStartB)
						TangentB *= -1.f;
					TOptional<FVector>* OverrideB = bIsStartB
						                                ? &ExtruderB->Overrides.StartTangent
						                                : &ExtruderB->Overrides.EndTangent;

					const FVector HalfwayTangent = (Tangent + TangentB).GetSafeNormal();
					*Override = HalfwayTangent;
					*OverrideB = HalfwayTangent;
					return true;
				};
				if (CheckNodeB(RuleB.Start))
					break;
				if (CheckNodeB(RuleB.End))
					break;
			}
		};
		CheckNode(Rule.Start);
		CheckNode(Rule.End);
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
