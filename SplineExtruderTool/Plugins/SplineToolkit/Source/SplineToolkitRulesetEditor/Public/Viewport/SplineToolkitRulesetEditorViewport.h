// Fill out your copyright notice in the Description page of Project Settings.

// Based on:
// https://github.com/shaderwitch/AssetEditorTemplate

#pragma once

#include "CoreMinimal.h"
#include "AdvancedPreviewScene.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SEditorViewport.h"

class USplineToolkitRuleset;
class FSplineToolkitRulesetPreviewScene;
class FSplineToolkitRulesetEditorToolkit;

struct FSplineToolkitRulesetViewportRequiredArgs
{
	FSplineToolkitRulesetViewportRequiredArgs(const TSharedRef<class FSplineToolkitRulesetPreviewScene>& InPreviewScene, TSharedRef<class FAssetEditorToolkit> InAssetEditorToolkit, int32 InViewportIndex)
	: PreviewScene(InPreviewScene)
	, AssetEditorToolkit(InAssetEditorToolkit)
	, ViewportIndex(InViewportIndex)
	{
	}


	TSharedRef<class FSplineToolkitRulesetPreviewScene> PreviewScene;
	TSharedRef<class FAssetEditorToolkit> AssetEditorToolkit;
	int32 ViewportIndex;
};

class FSplineToolkitRulesetViewportClient;
/**
 * 
 */
class SSplineToolkitRulesetViewport : public SEditorViewport, public FGCObject, public ICommonEditorViewportToolbarInfoProvider
{
public:

	SLATE_BEGIN_ARGS(SSplineToolkitRulesetViewport) {}
	SLATE_END_ARGS()

	/** The scene for this viewport. */
	TSharedPtr<FSplineToolkitRulesetPreviewScene> PreviewScene;

	void Construct(const FArguments& InArgs, TSharedPtr<FSplineToolkitRulesetEditorToolkit> InShowcaseAssetEditor, TSharedPtr<FSplineToolkitRulesetPreviewScene> InPreviewScene);
	virtual ~SSplineToolkitRulesetViewport() override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override {}

	virtual FString GetReferencerName() const override;
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	
	virtual void OnFocusViewportToSelection() override;

	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

	TSharedPtr<class FSplineToolkitRulesetViewportClient> GetViewportClient() { return TypedViewportClient; };
	
	//Shared ptr to the client
	TSharedPtr<class FSplineToolkitRulesetViewportClient> TypedViewportClient;

	//Toolkit Pointer
	TSharedPtr<FSplineToolkitRulesetEditorToolkit> EditorPtr;
	
};