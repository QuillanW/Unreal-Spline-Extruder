#include "SplineToolkitInstantiator.h"
#include "SplineToolkitMapEditor.h"
#include "SplineToolkitMeshExtruder.h"

void USplineToolkitEditMode::SelectClosestSpline(FVector ClickPos)
{
	GEditor->SelectNone(true, true);
	
	USplineComponent* ClosestComp = FindClosestSpline(ClickPos);
	if (!ClosestComp) return;
	
	LastSelectedSpline = SelectedSpline;
	SelectedSpline = ClosestComp;
	GEditor->SelectComponent(ClosestComp, true, true, true);
	CurrentEditMode = ESplineToolkitEditorModes::None;
}

void USplineToolkitEditMode::ConnectSplines(FVector ClickPos)
{
	GEditor->SelectNone(true, true);
	
}

void USplineToolkitEditMode::DisconnectSplines(FVector ClickPos)
{
	GEditor->SelectNone(true, true);
}

void USplineToolkitEditMode::UpdateRuleset(FVector ClickPos)
{
	GEditor->SelectNone(true, true);
	USplineComponent* ClosestComp = FindClosestSpline(ClickPos);
	if (!ClosestComp) return;
	
	const auto Actor = ClosestComp->GetOwner();
	Actor->GetComponentByClass<USplineToolkitInstantiator>()->Ruleset = SelectedRuleset;
	Actor->GetComponentByClass<USplineToolkitMeshExtruder>()->Ruleset = SelectedRuleset;
	Actor->GetComponentByClass<USplineToolkitIntersectionSolver>()->Ruleset = SelectedRuleset;
	// TODO: Add more components
	
	ClosestComp->GetOnSplineChanged().Broadcast();
}
