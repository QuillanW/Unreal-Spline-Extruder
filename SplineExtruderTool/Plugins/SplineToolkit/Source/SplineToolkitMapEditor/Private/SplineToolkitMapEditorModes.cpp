#include "EngineUtils.h"
#include "SplineToolkitMapEditor.h"

void USplineToolkitEditMode::SelectClosestSpline(FVector ClickPos)
{
	GEditor->SelectNone(true, true);
	
	float ClosestDistance = 1000.0f;
	USplineComponent* ClosestComp = nullptr;
	
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor) continue;
		USplineComponent* SplineComp = Actor->GetComponentByClass<USplineComponent>();
		if (!SplineComp) continue;
		FVector WorldLoc = SplineComp->GetWorldLocationAtDistanceAlongSpline(SplineComp->GetDistanceAlongSplineAtLocation(ClickPos, ESplineCoordinateSpace::World));
		float Dist = (ClickPos - WorldLoc).Length();
		if (Dist < ClosestDistance)
		{
			ClosestDistance = Dist;
			ClosestComp = SplineComp;
		}
	}
	
	if (!ClosestComp) return;
	
	LastSelectedSpline = SelectedSpline;
	SelectedSpline = ClosestComp;
	GEditor->SelectComponent(ClosestComp, true, true, true);
	CurrentEditMode = ESplineToolkitEditorModes::None;
}

void USplineToolkitEditMode::ConnectSplines(FVector ClickPos)
{
}

void USplineToolkitEditMode::DisconnectSplines(FVector ClickPos)
{
}

void USplineToolkitEditMode::UpdateRuleset(FVector ClickPos)
{
}
