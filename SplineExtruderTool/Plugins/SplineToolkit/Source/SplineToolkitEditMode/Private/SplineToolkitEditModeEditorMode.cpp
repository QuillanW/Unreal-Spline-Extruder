// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineToolkitEditModeEditorMode.h"
#include "SplineToolkitEditModeEditorModeToolkit.h"
#include "EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "SplineToolkitEditModeEditorModeCommands.h"
#include "Modules/ModuleManager.h"

#include "Styling/AppStyle.h"
#include "Tools/SplineToolkitEditModeSimpleTool.h"
#include "Tools/SplineToolkitEditModeInteractiveTool.h"


#define LOCTEXT_NAMESPACE "SplineToolkitEditModeEditorMode"

const FEditorModeID USplineToolkitEditModeEditorMode::EM_SplineToolkitEditModeEditorModeId = TEXT("EM_SplineToolkitEditModeEditorMode");

FString USplineToolkitEditModeEditorMode::SimpleToolName = TEXT("SplineToolkitEditMode_ActorInfoTool");
FString USplineToolkitEditModeEditorMode::InteractiveToolName = TEXT("SplineToolkitEditMode_MeasureDistanceTool");


USplineToolkitEditModeEditorMode::USplineToolkitEditModeEditorMode()
{
	FModuleManager::Get().LoadModule("EditorStyle");

	Info = FEditorModeInfo(USplineToolkitEditModeEditorMode::EM_SplineToolkitEditModeEditorModeId,
		LOCTEXT("ModeName", "Spline"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "TimelineEditor.AddCurveAssetTrack"),
		true);
}


USplineToolkitEditModeEditorMode::~USplineToolkitEditModeEditorMode()
{
}


void USplineToolkitEditModeEditorMode::ActorSelectionChangeNotify()
{
}

void USplineToolkitEditModeEditorMode::Enter()
{
	UEdMode::Enter();
	
	const FSplineToolkitEditModeEditorModeCommands& SampleToolCommands = FSplineToolkitEditModeEditorModeCommands::Get();

	RegisterTool(SampleToolCommands.SimpleTool, SimpleToolName, NewObject<USplineToolkitEditModeSimpleToolBuilder>(this));
	RegisterTool(SampleToolCommands.InteractiveTool, InteractiveToolName, NewObject<USplineToolkitEditModeInteractiveToolBuilder>(this));

	GetToolManager()->SelectActiveToolType(EToolSide::Left, SimpleToolName);
}

void USplineToolkitEditModeEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FSplineToolkitEditModeEditorModeToolkit);
}

static bool HasSpline(const AActor* Actor)
{
	return Actor && Actor->FindComponentByClass<USplineComponent>() != nullptr;
}

bool USplineToolkitEditModeEditorMode::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	return !bInSelection || HasSpline(InActor);
}

bool USplineToolkitEditModeEditorMode::IsSelectionDisallowed(AActor* InActor, bool bInSelection) const
{
	return bInSelection && !HasSpline(InActor);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> USplineToolkitEditModeEditorMode::GetModeCommands() const
{
	return FSplineToolkitEditModeEditorModeCommands::Get().GetCommands();
}

#undef LOCTEXT_NAMESPACE
