// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineToolkitEditModeEditorModeToolkit.h"
#include "SplineToolkitEditModeEditorMode.h"
#include "Engine/Selection.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "SplineToolkitEditModeEditorModeToolkit"

FSplineToolkitEditModeEditorModeToolkit::FSplineToolkitEditModeEditorModeToolkit()
{
}

void FSplineToolkitEditModeEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);
}

void FSplineToolkitEditModeEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add(NAME_Default);
}


FName FSplineToolkitEditModeEditorModeToolkit::GetToolkitFName() const
{
	return FName("Spline");
}

FText FSplineToolkitEditModeEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "Spline Editing Toolkit");
}

#undef LOCTEXT_NAMESPACE
