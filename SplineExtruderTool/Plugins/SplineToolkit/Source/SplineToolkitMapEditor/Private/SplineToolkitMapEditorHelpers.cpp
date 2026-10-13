#include "EngineUtils.h"
#include "SplineToolkitMapEditor.h"

USplineComponent* USplineToolkitEditMode::FindClosestSpline(const FVector& Pos, bool& EndClosest) const
{
	float ClosestDistance = 1000.0f;
	USplineComponent* ClosestComp = nullptr;
	
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor) continue;
		USplineComponent* SplineComp = Actor->GetComponentByClass<USplineComponent>();
		if (!SplineComp) continue;
		FVector WorldLoc = SplineComp->GetWorldLocationAtDistanceAlongSpline(SplineComp->GetDistanceAlongSplineAtLocation(Pos, ESplineCoordinateSpace::World));
		float Dist = (Pos - WorldLoc).Length();
		if (Dist < ClosestDistance)
		{
			ClosestDistance = Dist;
			ClosestComp = SplineComp;
		}
	}
	
	if (!ClosestComp) return nullptr;
	
	FVector StartLoc = ClosestComp->GetWorldLocationAtTime(0.0f, ESplineCoordinateSpace::World);
	float StartDist = (Pos - StartLoc).Length();
	FVector EndLoc = ClosestComp->GetWorldLocationAtTime(1.0f, ESplineCoordinateSpace::World);
	float EndDist = (Pos - EndLoc).Length();
	EndClosest = EndDist < StartDist;
	
	return ClosestComp;
}

USplineComponent* USplineToolkitEditMode::FindClosestSpline(const FVector& Pos) const
{
	bool _;
	return FindClosestSpline(Pos, _);
}
