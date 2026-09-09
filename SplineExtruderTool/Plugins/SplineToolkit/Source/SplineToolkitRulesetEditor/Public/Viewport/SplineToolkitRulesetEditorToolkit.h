// Fill out your copyright notice in the Description page of Project Settings.

// Based on:
// https://github.com/shaderwitch/AssetEditorTemplate

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"

class FSplineToolkitRulesetPreviewScene;
class SSplineToolkitRulesetViewport;
class USplineToolkitRuleset;
/**
 * 
 */
class FSplineToolkitRulesetEditorToolkit : public FAssetEditorToolkit, public FNotifyHook
{
public:
	FSplineToolkitRulesetEditorToolkit();
	virtual ~FSplineToolkitRulesetEditorToolkit() override;

	void InitAssetEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		USplineToolkitRuleset* InSplineToolkitRuleset);

	/* Simple Asset Editor methods */
	void BindCommands();
	void ExtendToolbars();
	
	void FocusViewport() const;
	void ToggleAutoUpdate();
	
	TSharedPtr<FSplineToolkitRulesetPreviewScene> CreatePreviewScene();
	/* End Simple Asset Editor methods */
	
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	
	/** IToolkit interface */
	virtual FName GetToolkitFName() const override { return "SplineToolkitRulesetEditor"; };
	virtual FText GetBaseToolkitName() const override { return INVTEXT("Spline Toolkit Ruleset Editor"); };
	virtual FString GetWorldCentricTabPrefix() const override { return "Spline Toolkit Ruleset "; };
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return {}; };
	/** End IToolkit interface */
	
	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args) const;
	
	USplineToolkitRuleset* GetRuleset() const { return SplineToolkitRuleset; }

private:

	USplineToolkitRuleset* SplineToolkitRuleset;
	TSharedPtr<FSplineToolkitRulesetPreviewScene> PreviewScene;
	TSharedPtr<SSplineToolkitRulesetViewport> PreviewViewportWidget;
protected:
	void OnClose() override;

};