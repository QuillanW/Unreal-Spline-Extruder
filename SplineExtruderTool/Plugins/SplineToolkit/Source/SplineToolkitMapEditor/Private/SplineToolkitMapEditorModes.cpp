#include "SplineToolkitConnector.h"
#include "SplineToolkitInstantiator.h"
#include "SplineToolkitMapEditor.h"

void USplineToolkitEditMode::CreateNewSpline(FVector ClickPos)
{
	GEditor->SelectNone(true, true);
	
	if (LastPoint == FVector::ZeroVector)
	{
		DrawDebugSphere(GetWorld(), ClickPos, 32.0f, 16, FColor::Orange, false, 1.0f);
		LastPoint = ClickPos;
		return;
	}
	
	auto* Actor = GetWorld()->SpawnActor(AActor::StaticClass());
	AddSpline(Actor);
	auto Spline = Actor->GetComponentByClass<USplineComponent>();
	Actor->SetRootComponent(Spline);
	Spline->SetSplinePoints({LastPoint, ClickPos}, ESplineCoordinateSpace::World);
	
	LastPoint = FVector::ZeroVector;
	CurrentEditMode = ESplineToolkitEditorModes::None;
	
	VerifyComponents(Spline);
	SetRuleset(Spline);
	
	LastSelectedSpline = SelectedSpline;
	SelectedSpline = Spline;
	GEditor->SelectComponent(Spline, true, true, true);
}

void USplineToolkitEditMode::SelectClosestSpline(FVector ClickPos)
{
	GEditor->SelectNone(true, true);
	
	USplineComponent* ClosestComp = FindClosestSpline(ClickPos);
	if (!ClosestComp) return;
	
	LastSelectedSpline = SelectedSpline;
	SelectedSpline = ClosestComp;
	GEditor->SelectComponent(ClosestComp, true, true, true);
	CurrentEditMode = ESplineToolkitEditorModes::None;
	VerifyComponents(ClosestComp);
}

void USplineToolkitEditMode::ConnectSplines(FVector ClickPos)
{
	GEditor->SelectNone(true, true);
	
	if (LastSelectedSpline)
	{
		bool bFromEnd = false;
		auto FromSpline = FindClosestSpline(ClickPos, bFromEnd);
		if (FromSpline == LastSelectedSpline) return;
		
		auto Connector = FromSpline->GetOwner()->GetComponentByClass<USplineToolkitConnector>();
		if (!Connector) 
			return;
		Connector->Attach({LastSelectedSpline, bFromEnd, bLastSelectedSplineEnd, true });
		
		LastSelectedSpline = nullptr;
	}
	else
	{
		LastSelectedSpline = FindClosestSpline(ClickPos, bLastSelectedSplineEnd);
		DrawDebugSphere(GetWorld(), LastSelectedSpline->GetWorldLocationAtTime(bLastSelectedSplineEnd ? 1.0f : 0.0f), 32.0f, 16, FColor::Orange, false, 1.0f);
	}
}

void USplineToolkitEditMode::DisconnectSplines(FVector ClickPos)
{
	GEditor->SelectNone(true, true);
	
	if (LastSelectedSpline)
	{
		bool bFromEnd = false;
		auto FromSpline = FindClosestSpline(ClickPos, bFromEnd);
		if (FromSpline == LastSelectedSpline) return;
		
		auto Connector = FromSpline->GetOwner()->GetComponentByClass<USplineToolkitConnector>();
		if (!Connector) 
			return;
		Connector->Detach(LastSelectedSpline, bFromEnd ? ESplineEnd::End : ESplineEnd::Start);
		
		LastSelectedSpline = nullptr;
	}
	else 
	{
		LastSelectedSpline = FindClosestSpline(ClickPos, bLastSelectedSplineEnd);
		if (LastSelectedSpline)
			DrawDebugSphere(GetWorld(), LastSelectedSpline->GetWorldLocationAtTime(bLastSelectedSplineEnd ? 1.0f : 0.0f), 32.0f, 16, FColor::Orange, false, 1.0f);
	}
}

void USplineToolkitEditMode::UpdateRuleset(FVector ClickPos)
{
	GEditor->SelectNone(true, true);
	USplineComponent* ClosestComp = FindClosestSpline(ClickPos);
	if (!ClosestComp) return;
	SetRuleset(ClosestComp);
}
