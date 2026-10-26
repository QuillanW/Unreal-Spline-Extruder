#include "EngineUtils.h"
#include "SplineToolkitConnector.h"
#include "SplineToolkitFiller.h"
#include "SplineToolkitInstantiator.h"
#include "SplineToolkitMapEditor.h"
#include "SplineToolkitMeshExtruder.h"
#include "SplineToolkitMeshStretcher.h"

USplineComponent* USplineToolkitEditMode::FindClosestSpline(const FVector& Pos, bool& EndClosest) const
{
	float             ClosestDistance = 1000.0f;
	USplineComponent* ClosestComp     = nullptr;

	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor) continue;
		USplineComponent* SplineComp = Actor->GetComponentByClass<USplineComponent>();
		if (!SplineComp) continue;
		FVector WorldLoc = SplineComp->GetWorldLocationAtDistanceAlongSpline(
			SplineComp->GetDistanceAlongSplineAtLocation(Pos, ESplineCoordinateSpace::World));
		float Dist = (Pos - WorldLoc).Length();
		if (Dist < ClosestDistance)
		{
			ClosestDistance = Dist;
			ClosestComp     = SplineComp;
		}
	}

	if (!ClosestComp) return nullptr;

	FVector StartLoc  = ClosestComp->GetWorldLocationAtTime(0.0f, ESplineCoordinateSpace::World);
	float   StartDist = (Pos - StartLoc).Length();
	FVector EndLoc    = ClosestComp->GetWorldLocationAtTime(1.0f, ESplineCoordinateSpace::World);
	float   EndDist   = (Pos - EndLoc).Length();
	EndClosest        = EndDist < StartDist;

	return ClosestComp;
}

USplineComponent* USplineToolkitEditMode::FindClosestSpline(const FVector& Pos) const
{
	bool _;
	return FindClosestSpline(Pos, _);
}

void USplineToolkitEditMode::VerifyComponents(const USplineComponent* Spline)
{
	AActor* Actor = Spline->GetOwner();

	// Me having to make this lambda is saying something about the engine
	auto AddComponent = [this, &Actor]<typename TComponent>() -> TComponent*
	{
		TComponent* Comp = Actor->FindComponentByClass<TComponent>();
		if (!Comp)
		{
			Comp                 = NewObject<TComponent>(Actor, NAME_None, RF_Transactional);
			Comp->CreationMethod = EComponentCreationMethod::Instance;
			if constexpr (std::is_base_of_v<USceneComponent, TComponent>)
				Comp->SetupAttachment(Actor->GetRootComponent());
			Comp->RegisterComponent();
			Actor->AddInstanceComponent(Comp);
		}
		return Comp;
	};

	AddComponent.operator()<USplineToolkitConnector>();
	AddComponent.operator()<USplineToolkitInstantiator>();
	AddComponent.operator()<USplineToolkitMeshExtruder>();
	AddComponent.operator()<USplineToolkitMeshStretcher>();
	AddComponent.operator()<USplineToolkitRmfSampler>();
	AddComponent.operator()<USplineToolkitFiller>();
	AddComponent.operator()<USplineToolkitIntersectionSolver>();
	// TODO: Add more components
}

void USplineToolkitEditMode::AddSpline(AActor* Actor)
{
	auto AddComponent = [this, &Actor]<typename TComponent>() -> TComponent*
	{
		TComponent* Comp = Actor->FindComponentByClass<TComponent>();
		if (!Comp)
		{
			Comp                 = NewObject<TComponent>(Actor, NAME_None, RF_Transactional);
			Comp->CreationMethod = EComponentCreationMethod::Instance;
			if constexpr (std::is_base_of_v<USceneComponent, TComponent>)
				Comp->SetupAttachment(Actor->GetRootComponent());
			Comp->RegisterComponent();
			Actor->AddInstanceComponent(Comp);
		}
		return Comp;
	};
	AddComponent.operator()<USplineComponent>();
}

void USplineToolkitEditMode::SetRuleset(USplineComponent* Spline) const
{
	AActor* Actor                                                           = Spline->GetOwner();
	Actor->GetComponentByClass<USplineToolkitInstantiator>()->Ruleset       = SelectedRuleset;
	Actor->GetComponentByClass<USplineToolkitMeshExtruder>()->bUpdateOnSplineChange       = true;
	Actor->GetComponentByClass<USplineToolkitMeshExtruder>()->Ruleset       = SelectedRuleset;
	Actor->GetComponentByClass<USplineToolkitMeshStretcher>()->Ruleset      = SelectedRuleset;
	Actor->GetComponentByClass<USplineToolkitIntersectionSolver>()->Ruleset = SelectedRuleset;
	Actor->GetComponentByClass<USplineToolkitFiller>()->Ruleset             = SelectedRuleset;
	Actor->GetComponentByClass<USplineToolkitFiller>()->bUpdateOnSplineChange       = true;
	// TODO: Add more components
	Spline->GetOnSplineChanged().Broadcast();
}
