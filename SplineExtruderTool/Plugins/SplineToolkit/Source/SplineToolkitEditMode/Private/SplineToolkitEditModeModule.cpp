// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineToolkitEditModeModule.h"
#include "SplineToolkitEditModeEditorModeCommands.h"

#define LOCTEXT_NAMESPACE "SplineToolkitEditModeModule"

void FSplineToolkitEditModeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FSplineToolkitEditModeEditorModeCommands::Register();
}

void FSplineToolkitEditModeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FSplineToolkitEditModeEditorModeCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSplineToolkitEditModeModule, SplineToolkitEditMode)