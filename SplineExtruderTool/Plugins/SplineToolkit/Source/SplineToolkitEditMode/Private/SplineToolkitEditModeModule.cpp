// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineToolkitEditModeModule.h"

#include "SplineToolkitEditExtension.h"
#include "SplineToolkitEditModeEditorModeCommands.h"
#include "UnrealEdGlobals.h"
#include "Components/SplineComponent.h"
#include "Editor/UnrealEdEngine.h"

#define LOCTEXT_NAMESPACE "SplineToolkitEditModeModule"

void FSplineToolkitEditModeModule::StartupModule()
{
	if (GUnrealEd)
	{
		// Override the normal spline visualizer so we can use our modified version
		GUnrealEd->UnregisterComponentVisualizer(USplineComponent::StaticClass()->GetFName());

		SplineEditExtension = MakeShareable(new FSplineToolkitEditExtension());
		GUnrealEd->RegisterComponentVisualizer(USplineComponent::StaticClass()->GetFName(), SplineEditExtension);
		SplineEditExtension->OnRegister();
	}
	
	FSplineToolkitEditModeEditorModeCommands::Register();
}

void FSplineToolkitEditModeModule::ShutdownModule()
{
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(USplineComponent::StaticClass()->GetFName());
	}

	FSplineToolkitEditModeEditorModeCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSplineToolkitEditModeModule, SplineToolkitEditMode)