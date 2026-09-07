#pragma once

// Based on:
// https://github.com/shaderwitch/AssetEditorTemplate


#include "CoreMinimal.h"
#include "AdvancedPreviewScene.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "Editor/UnrealEd/Public/SCommonEditorViewportToolbarBase.h"

class FSplineToolkitRulesetPreviewScene;
class FSplineToolkitRulesetEditorToolkit;
class SSplineToolkitRulesetViewport;
/**
 * FEditorViewportClient is generally responsible for handling the viewport, camera movement, and
 * any of the options you'd find under the "Lit" menu on the standard Unreal Engine main viewport.
 */
class FSplineToolkitRulesetViewportClient : public FEditorViewportClient
{
public:

	/** Pointer back to the Editor Viewport */
	TWeakPtr<class SEditorViewport> ViewportPtr;

	/* Constructor and destructor */
	FSplineToolkitRulesetViewportClient(const TSharedRef<SEditorViewport>& InThumbnailViewport, const TSharedRef<FAdvancedPreviewScene>& InPreviewScene);

	/* Shameless stolen from SMaterialEditorViewport because for some reason a basic Focus on Selection is not implemented in the ViewportClient base class. */
	void FocusViewportOnBounds(const FBoxSphereBounds Bounds, bool bInstant /*= false*/);

	/* Stored pointer to the preview scene in which the static mesh is shown */
	FAdvancedPreviewScene* AdvancedPreviewScene;
};