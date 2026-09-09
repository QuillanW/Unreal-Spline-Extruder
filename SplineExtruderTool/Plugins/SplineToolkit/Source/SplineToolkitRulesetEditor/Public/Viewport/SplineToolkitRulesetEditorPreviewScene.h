// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AdvancedPreviewScene.h"
#include "SplineToolkitRulesetEditorToolkit.h"

/**
 * 
 */
class FSplineToolkitRulesetPreviewScene : public FAdvancedPreviewScene
{
public:
	FSplineToolkitRulesetPreviewScene(ConstructionValues CVS, const TSharedRef<FSplineToolkitRulesetEditorToolkit>& EditorToolkit);
	virtual ~FSplineToolkitRulesetPreviewScene() override;

	virtual void Tick(float InDeltaTime) override;
	
	void UpdatePreview();

	TSharedRef<FSplineToolkitRulesetEditorToolkit> GetEditor() const
	{
		return EditorPtr.Pin().ToSharedRef();
	}

	TArray<AActor*> PreviewActors;
	
	bool AutoUpdate = true;
	
private:
	TWeakPtr<FSplineToolkitRulesetEditorToolkit> EditorPtr;
};