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

	for (const auto& Intersection : IntersectionSolver->Collisions)
	{
		auto Collision = DetermineCollisionPoints(Intersection);
		if (Collision.IsEmpty())
			continue;
		for (const auto& Point : Collision)
			this->DebugData.Add(Point.Midpoint);

		// Store the other intersection for reference
		auto* SolverB = Intersection.Other->GetOwner()->FindComponentByClass<USplineToolkitIntersectionSolver>();
		if (!SolverB)
			continue;

		auto* IntersectionB = SolverB->Collisions.FindByPredicate(
			[&](const FSplineToolkitSplineIntersection& TestIntersection)
			{
				return TestIntersection.Other == this->SplineComponent && (TestIntersection.Midpoint - Intersection.
					Midpoint).SquaredLength() < 100.f;
			});

		if (!IntersectionB)
			continue;

		const int32 SplineIndex = GetOwner()->GetUniqueID() < Intersection.Other->GetOwner()->GetUniqueID() ? 0 : 1;

		for (const auto& Rules : this->Ruleset->FillingRules)
		{
			if (!FMath::IsWithinInclusive(Intersection.AbsoluteAngleDifference, Rules.AngleMin, Rules.AngleMax))
				continue;
			RegenerateConnectRules(Collision, Intersection, *IntersectionB, SplineIndex, Rules);
			break;
		}
		if (auto* Filler = Intersection.Other->GetOwner()->FindComponentByClass<USplineToolkitFiller>();
			Filler && !Caller)
			Filler->RegenerateInternal(this);
	}
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
								return LenSq > KINDA_SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(P - S0, D) / LenSq, 0.f, 1.f) : 0.f;
							};

							OutHits.Add(FHitInfo{
								.Midpoint = MidPoint,
								.DistanceSquared = DistSq,
								.PolyLineA = I,
								.HitSampleA = SampleA + SegFraction(ClosestOnA, PolyA.Line[SampleA], PolyA.Line[SampleA + 1]),
								.PolyLineB = J,
								.HitSampleB = SampleB + SegFraction(ClosestOnB, PolyB.Line[SampleB], PolyB.Line[SampleB + 1]),
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
		Collision.DistanceA = FMath::Lerp(DistancesA.X, DistancesA.Y, Collision.DistanceA / static_cast<float>(NumSamples - 1));

		Collision.DistanceB /= Collision.TotalWeight;
		const auto& DistancesB = PolyLines[Collision.PolyLineB].DistanceRange;
		Collision.DistanceB = FMath::Lerp(DistancesB.X, DistancesB.Y, Collision.DistanceB / static_cast<float>(NumSamples - 1));
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


void USplineToolkitFiller::RegenerateConnectRules(const TArray<FCollisionPoint>& Collisions,
                                                  const FSplineToolkitSplineIntersection& Intersection,
                                                  const FSplineToolkitSplineIntersection& IntersectionB,
                                                  const int32 SplineIndex,
                                                  const FSplineToolkitFillingRules& Rules)
{
	int32 RuleIndex = 0;
	for (const auto& ConnectRule : Rules.ConnectRules)
	{
		if (ConnectRule.SplineIndex != SplineIndex)
		{
			++RuleIndex;
			continue;
		}

		switch (ConnectRule.RuleType)
		{
		case ESplineToolkitRuleType::INSTANTIATION:
		{
			const auto& Rule = this->Ruleset->InstantiationRules[ConnectRule.RuleIndex];
			RegenerateConnectRule(Collisions, Intersection, IntersectionB, SplineIndex, RuleIndex, ConnectRule,
			                      &Rule, USplineToolkitInstantiator::StaticClass());
			break;
		}
		case ESplineToolkitRuleType::EXTRUSION:
		{
			const auto& Rule = this->Ruleset->ExtrusionRules[ConnectRule.RuleIndex];
			RegenerateConnectRule(Collisions, Intersection, IntersectionB, SplineIndex, RuleIndex, ConnectRule,
			                      &Rule, USplineToolkitMeshExtruder::StaticClass());
			break;
		}
		case ESplineToolkitRuleType::STRETCH:
		{
			const auto& Rule = this->Ruleset->StretchRules[ConnectRule.RuleIndex];
			RegenerateConnectRule(Collisions, Intersection, IntersectionB, SplineIndex, RuleIndex, ConnectRule,
			                      &Rule, USplineToolkitMeshExtruder::StaticClass());
			break;
		}
		}

		++RuleIndex;
	}
}


void USplineToolkitFiller::RegenerateConnectRule(const TArray<FCollisionPoint>& Collisions,
                                                 const FSplineToolkitSplineIntersection& Intersection,
                                                 const FSplineToolkitSplineIntersection& IntersectionB,
                                                 int32 SplineIndex, int32 RuleIndex,
                                                 const FSplineToolkitFillingConnectRule& Rule,
                                                 const void* GeneratorRule, UClass* GeneratorClass)
{
	if (Rule.Start == Rule.End)
		return;

	if (RuleContinuesNormally(Rule))
	{
		auto* Comp = GetOwner()->FindComponentByClass(GeneratorClass);

		if (GeneratorClass == USplineToolkitInstantiator::StaticClass())
			Cast<USplineToolkitInstantiator>(Comp)->IgnoreIntersectCutouts.Emplace(Rule.RuleIndex, &Intersection);
		else if (GeneratorClass == USplineToolkitMeshExtruder::StaticClass())
			Cast<USplineToolkitMeshExtruder>(Comp)->IgnoreIntersectCutouts.Emplace(Rule.RuleIndex, &Intersection);

		return;
	}

	TObjectPtr<AActor> Actor = nullptr;
	if (auto* Found = SpawnedActors.Find(RuleIndex); Found && IsValid(*Found))
	{
		Actor = this->SpawnedActors[RuleIndex];
	}
	else
	{
		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Actor = GetWorld()->SpawnActor<AActor>(Params);
#if WITH_EDITOR
		Actor->SetActorLabel(FString::Printf(TEXT("FillingRule%d"), RuleIndex));
#endif
		this->SpawnedActors.Emplace(RuleIndex, Actor);
	}

	// Me having to make this lambda is saying something about the engine
	auto AddComponent = [&Actor]<typename TComponent>() -> TComponent*
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
		}
		return Comp;
	};

	// Add a spline component
	USplineComponent* SplineComp = AddComponent.operator()<USplineComponent>();

	// Helper function
	auto GetPositionTangentForNode = [&](const FSplineToolkitFillingConnectRuleParams& Params,
	                                     const TOptional<FVector>& Check) -> TTuple<FVector, FVector, float>
	{
		const FSplineToolkitSplineIntersection& Intersect = Rule.SplineIndex == SplineIndex
			                                                    ? Intersection
			                                                    : IntersectionB;
		USplineComponent* Spline = Rule.SplineIndex == SplineIndex ? this->SplineComponent : Intersection.Other;

		auto* Sampler = Spline->GetOwner()->FindComponentByClass<USplineToolkitRmfSampler>();
		if (!Sampler)
			return TTuple<FVector, FVector, float>{FVector::ZeroVector, FVector::ZeroVector, 0.f};

		switch (Params.Type)
		{
		case EFillAnchorType::CUTOUT_START:
		{
			const float Distance = Intersect.DistanceMin + Params.OffsetDistance;
			return TTuple<FVector, FVector, float>{
				Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
				Spline->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
				Distance
			};
		}
		case EFillAnchorType::CUTOUT_END:
		{
			const float Distance = Intersect.DistanceMax + Params.OffsetDistance;
			return TTuple<FVector, FVector, float>{
				Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
				Spline->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
				Distance
			};
		}
		case EFillAnchorType::INTERSECT_RULE:
		{
			TArray<TPair<float, const FCollisionPoint*>> Candidates{};

			for (const auto& Collision : Collisions)
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
					                        : Intersect.DistanceMin;

				const float Score = FMath::Abs(Dist - CheckDist);
				Candidates.Emplace(Score, &Collision);
			}
			Candidates.Sort();

			float Distance = 0.f;
			if (!Candidates.IsEmpty())
			{
				const auto* Candidate = Candidates[Params.IntersectionIndex].Value;
				DrawDebugSphere(GetWorld(), Candidate->Midpoint, 10.f, 16, FColor::Cyan);
				const bool bIsA = Candidate->SplineA == Spline;
				const bool bIsB = Candidate->SplineB == Spline;
				if (bIsA || bIsB)
					Distance = bIsA ? Candidate->DistanceA : Candidate->DistanceB;
			}
			Distance += Params.OffsetDistance;
			return TTuple<FVector, FVector, float>{
				Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
				Spline->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
				Distance
			};
		}
		}
		return TTuple<FVector, FVector, float>{FVector::ZeroVector, FVector::ZeroVector, 0.f};
	};
	auto [SplineStart, SplineLeaveTangent, DistanceStart] = GetPositionTangentForNode(Rule.Start, NullOpt);
	auto [SplineEnd, SplineArriveTangent, DistanceEnd] = GetPositionTangentForNode(Rule.End, SplineStart);
	SplineArriveTangent *= -1.f;

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
