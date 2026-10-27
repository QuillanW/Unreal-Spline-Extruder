// Based on:
// https://github.com/shaderwitch/AssetEditorTemplate

#pragma once

#include "CoreMinimal.h"
#include "AdvancedPreviewScene.h"
#include "SplineToolkitRulesetEditorToolkit.h"

enum class SplinePreview { Track, Loop, SBend, LongCurve };

/**
 * 
 */
class FSplineToolkitRulesetPreviewScene : public FAdvancedPreviewScene
{
public:
	FSplineToolkitRulesetPreviewScene(ConstructionValues CVS, const TSharedRef<FSplineToolkitRulesetEditorToolkit>& EditorToolkit);
	virtual ~FSplineToolkitRulesetPreviewScene() override;
	
	float GetPreviewScale() const { return PreviewScale; }
	void SetPreviewScale(float NewScale);

	virtual void Tick(float InDeltaTime) override;
	
	void UpdateSplinePreview(SplinePreview preview);
	
	void UpdatePreview();
	
	void SetPreviewSpline(SplinePreview Preview);
	
	SplinePreview GetCurrentPreview() const { return CurrentPreview; }

	TSharedRef<FSplineToolkitRulesetEditorToolkit> GetEditor() const
	{
		return EditorPtr.Pin().ToSharedRef();
	}

	TArray<TObjectPtr<AActor>> PreviewActors;
	
	bool AutoUpdate = true;
	
	SplinePreview CurrentPreview = SplinePreview::Track;
	
private:
	TWeakPtr<FSplineToolkitRulesetEditorToolkit> EditorPtr;
	
	float PreviewScale = 10.f;
};