// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Selection.h"
#include "Modules/ModuleManager.h"
#include "Tools/LegacyEdModeWidgetHelpers.h"
#include "Tools/UEdMode.h"
#include "SplineToolkitMapEditor.generated.h"

class FSplineWidgetHelper : public FLegacyEdModeWidgetHelper
{
public:
	virtual bool UsesTransformWidget() const override { return true; }
	virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const override { return true; }
	virtual bool ShouldDrawWidget() const override { return GEditor->GetSelectedActors()->Num() > 0; }
};


UCLASS()
class USplineOnlyEdMode : public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()
public:
	const static FEditorModeID EM_SplineOnlyEdModeId;

	USplineOnlyEdMode();

	virtual void Enter() override;
	virtual void Exit() override;

	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override;
	virtual bool IsSelectionDisallowed(AActor* InActor, bool bInSelection) const override;
	
	
protected:
	virtual TSharedRef<FLegacyEdModeWidgetHelper> CreateWidgetHelper() override
	{
		return MakeShared<FSplineWidgetHelper>();
	}
};

class FSplineToolkitMapEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
