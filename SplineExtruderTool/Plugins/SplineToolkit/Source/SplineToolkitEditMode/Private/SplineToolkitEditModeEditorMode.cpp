// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineToolkitEditModeEditorMode.h"
#include "SplineToolkitEditModeEditorModeToolkit.h"
#include "EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "SplineToolkitEditModeEditorModeCommands.h"
#include "Modules/ModuleManager.h"


//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
// AddYourTool Step 1 - include the header file for your Tools here
//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
#include "Styling/AppStyle.h"
#include "Tools/SplineToolkitEditModeSimpleTool.h"
#include "Tools/SplineToolkitEditModeInteractiveTool.h"

// step 2: register a ToolBuilder in FSplineToolkitEditModeEditorMode::Enter() below


#define LOCTEXT_NAMESPACE "SplineToolkitEditModeEditorMode"

const FEditorModeID USplineToolkitEditModeEditorMode::EM_SplineToolkitEditModeEditorModeId = TEXT("EM_SplineToolkitEditModeEditorMode");

FString USplineToolkitEditModeEditorMode::SimpleToolName = TEXT("SplineToolkitEditMode_ActorInfoTool");
FString USplineToolkitEditModeEditorMode::InteractiveToolName = TEXT("SplineToolkitEditMode_MeasureDistanceTool");


USplineToolkitEditModeEditorMode::USplineToolkitEditModeEditorMode()
{
	FModuleManager::Get().LoadModule("EditorStyle");

	// appearance and icon in the editing mode ribbon can be customized here
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

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// AddYourTool Step 2 - register the ToolBuilders for your Tools here.
	// The string name you pass to the ToolManager is used to select/activate your ToolBuilder later.
	//////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////// 
	const FSplineToolkitEditModeEditorModeCommands& SampleToolCommands = FSplineToolkitEditModeEditorModeCommands::Get();

	RegisterTool(SampleToolCommands.SimpleTool, SimpleToolName, NewObject<USplineToolkitEditModeSimpleToolBuilder>(this));
	RegisterTool(SampleToolCommands.InteractiveTool, InteractiveToolName, NewObject<USplineToolkitEditModeInteractiveToolBuilder>(this));

	// active tool type is not relevant here, we just set to default
	GetToolManager()->SelectActiveToolType(EToolSide::Left, SimpleToolName);
}

void USplineToolkitEditModeEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FSplineToolkitEditModeEditorModeToolkit);
}

bool USplineToolkitEditModeEditorMode::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	return IsValid(InActor->GetComponentByClass<USplineComponent>());
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> USplineToolkitEditModeEditorMode::GetModeCommands() const
{
	return FSplineToolkitEditModeEditorModeCommands::Get().GetCommands();
}

#undef LOCTEXT_NAMESPACE
