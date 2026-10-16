// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Selection.h"
#include "SplineToolkitRuleset.h"
#include "Components/SplineComponent.h"
#include "Modules/ModuleManager.h"
#include "Tools/LegacyEdModeWidgetHelpers.h"
#include "Tools/UEdMode.h"
#include "Widgets/Input/SButton.h"
#include "SplineToolkitMapEditor.generated.h"

enum class ESplineToolkitEditorModes : uint8 { None, Create, Select, Connect, Disconnect, UpdateRuleset };

// Widget helper forces the gizmo to be allowed enabled
class FSplineWidgetHelper : public FLegacyEdModeWidgetHelper
{
public:
	virtual bool UsesTransformWidget() const override { return true; }
	virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const override { return true; }
	virtual bool ShouldDrawWidget() const override { return GEditor->GetSelectedActors()->Num() > 0; }
};

UCLASS()
class USplineToolkitEditMode : public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()
public:
	const static FEditorModeID EM_USplineToolkitEditModeID;

	USplineToolkitEditMode();

	virtual void Enter() override;
	virtual void Exit() override;
	
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;

	virtual bool UsesToolkits() const override { return true; }
	
	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override;
	virtual bool IsSelectionDisallowed(AActor* InActor, bool bInSelection) const override;

	void SetEditMode(ESplineToolkitEditorModes Mode);
	ESplineToolkitEditorModes CurrentMode() const { return CurrentEditMode; }
	
	TObjectPtr<USplineToolkitRuleset> SelectedRuleset;
	
protected:
	virtual void CreateToolkit() override;
	
	virtual TSharedRef<FLegacyEdModeWidgetHelper> CreateWidgetHelper() override
	{
		return MakeShared<FSplineWidgetHelper>();
	}
	
	USplineComponent* FindClosestSpline(const FVector& Pos, bool& EndClosest) const;
	USplineComponent* FindClosestSpline(const FVector& Pos) const;
	void VerifyComponents(const USplineComponent* Spline);
	void AddSpline(AActor* Actor);
	void SetRuleset(USplineComponent* Spline) const;
	
	virtual void CreateNewSpline(FVector ClickPos);
	virtual void SelectClosestSpline(FVector ClickPos);
	virtual void ConnectSplines(FVector ClickPos);
	virtual void DisconnectSplines(FVector ClickPos);
	virtual void UpdateRuleset(FVector ClickPos);
	
	
	ESplineToolkitEditorModes CurrentEditMode = ESplineToolkitEditorModes::Select;
	UPROPERTY()
	TObjectPtr<USplineComponent> SelectedSpline = nullptr;
	UPROPERTY()
	TObjectPtr<USplineComponent> LastSelectedSpline = nullptr;
	FVector LastPoint = FVector::ZeroVector;
	bool bLastSelectedSplineEnd = false;
	
	TSharedPtr<SWidget> ViewportPanel;
};

// Toolkit defines the mode selection buttons
class FSplineToolkitModeToolkit : public FModeToolkit
{
public:
	explicit FSplineToolkitModeToolkit(USplineToolkitEditMode* InMode) : Mode(InMode) {}

	virtual FName GetToolkitFName() const override { return "SplineToolkitMode"; }
	virtual FText GetBaseToolkitName() const override { return INVTEXT("Spline Toolkit"); }

	virtual TSharedPtr<SWidget> GetInlineContent() const override;

private:
	TWeakObjectPtr<USplineToolkitEditMode> Mode;

	TSharedRef<SWidget> MakeButton(const FText& Label, ESplineToolkitEditorModes BtnMode) const;
	TSharedRef<SWidget> MakeRulesetPicker() const;
};

class FSplineToolkitMapEditorModule : public IModuleInterface
{
public:

	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};
