// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineToolkitMapEditor.h"

#include "EditorModeRegistry.h"
#include "Components/SplineComponent.h"

#define LOCTEXT_NAMESPACE "FSplineToolkitMapEditorModule"

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSplineToolkitMapEditorModule, SplineToolkitMapEditor)

// SplineOnlyEdMode.cpp
const FEditorModeID USplineOnlyEdMode::EM_SplineOnlyEdModeId = TEXT("EM_SplineOnlyEdMode");

void USplineOnlyEdMode::Enter()
{
	Super::Enter();
}

void USplineOnlyEdMode::Exit()
{
	Super::Exit();
}

USplineOnlyEdMode::USplineOnlyEdMode()
{
	Info = FEditorModeInfo(EM_SplineOnlyEdModeId, INVTEXT("Spline Editor"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SplineComponent"), true);
}

static bool HasSpline(const AActor* Actor)
{
	return Actor && Actor->FindComponentByClass<USplineComponent>() != nullptr;
}

bool USplineOnlyEdMode::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	// Always allow deselecting, only allow selecting actors that have a spline
	return !bInSelection || HasSpline(InActor);
}

bool USplineOnlyEdMode::IsSelectionDisallowed(AActor* InActor, bool bInSelection) const
{
	return bInSelection && !HasSpline(InActor);
}