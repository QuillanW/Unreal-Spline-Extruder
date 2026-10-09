// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Selection.h"
#include "Tools/LegacyEdModeWidgetHelpers.h"
#include "Tools/UEdMode.h"
#include "SplineToolkitEditModeEditorMode.generated.h"


// The helper forces the gizmo enabled
class FSplineWidgetHelper : public FLegacyEdModeWidgetHelper
{
public:
	virtual bool UsesTransformWidget() const override { return true; }
	virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const override { return true; }
	virtual bool ShouldDrawWidget() const override { return GEditor->GetSelectedActors()->Num() > 0; }
};

UCLASS()
class USplineToolkitEditModeEditorMode : public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()

public:
	const static FEditorModeID EM_SplineToolkitEditModeEditorModeId;

	static FString SimpleToolName;
	static FString InteractiveToolName;

	USplineToolkitEditModeEditorMode();
	virtual ~USplineToolkitEditModeEditorMode();

	/** UEdMode interface */
	virtual void Enter() override;
	virtual void ActorSelectionChangeNotify() override;
	
	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override;
	virtual bool IsSelectionDisallowed(AActor* InActor, bool bInSelection) const override;
	
	virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;
	
protected:
	virtual void CreateToolkit() override;
	
	virtual TSharedRef<FLegacyEdModeWidgetHelper> CreateWidgetHelper() override
	{
		return MakeShared<FSplineWidgetHelper>();
	}
};
