#include "SplineToolkitConnector.h"
#include "SplineToolkitFiller.h"
#include "SplineToolkitInstantiator.h"
#include "SplineToolkitMapEditor.h"
#include "SplineToolkitMeshExtruder.h"
#include "SplineToolkitMeshStretcher.h"

void USplineToolkitEditMode::SelectClosestSpline(FVector ClickPos)
{
	GEditor->SelectNone(true, true);
	
	USplineComponent* ClosestComp = FindClosestSpline(ClickPos);
	if (!ClosestComp) return;
	
	LastSelectedSpline = SelectedSpline;
	SelectedSpline = ClosestComp;
	GEditor->SelectComponent(ClosestComp, true, true, true);
	CurrentEditMode = ESplineToolkitEditorModes::None;
	
	auto Actor = ClosestComp->GetOwner();
	
	// Me having to make this lambda is saying something about the engine
	auto AddComponent = [this, &Actor]<typename TComponent>() -> TComponent*
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
	
	AddComponent.operator()<USplineToolkitConnector>();
	AddComponent.operator()<USplineToolkitInstantiator>();
	AddComponent.operator()<USplineToolkitMeshExtruder>();
	AddComponent.operator()<USplineToolkitMeshStretcher>();
	AddComponent.operator()<USplineToolkitRmfSampler>();
	AddComponent.operator()<USplineToolkitFiller>();
	AddComponent.operator()<USplineToolkitIntersectionSolver>();
	// TODO: Add more components
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
		DrawDebugSphere(GetWorld(), LastSelectedSpline->GetWorldLocationAtTime(bLastSelectedSplineEnd ? 1.0f : 0.0f), 32.0f, 16, FColor::Orange, true, 10.0f);
	}
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
	Actor->GetComponentByClass<USplineToolkitMeshStretcher>()->Ruleset = SelectedRuleset;
	Actor->GetComponentByClass<USplineToolkitIntersectionSolver>()->Ruleset = SelectedRuleset;
	Actor->GetComponentByClass<USplineToolkitFiller>()->Ruleset = SelectedRuleset;
	// TODO: Add more components
	
	ClosestComp->GetOnSplineChanged().Broadcast();
}
