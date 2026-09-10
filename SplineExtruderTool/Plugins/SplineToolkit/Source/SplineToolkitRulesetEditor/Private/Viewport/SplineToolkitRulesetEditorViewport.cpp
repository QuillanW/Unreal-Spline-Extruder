// Based on:
// https://github.com/shaderwitch/AssetEditorTemplate


#include "Viewport/SplineToolkitRulesetEditorViewport.h"

#include "SplineToolkitRuleset.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Viewport/SplineToolkitRulesetEditorPreviewScene.h"
#include "Viewport/SplineToolkitRulesetEditorViewportClient.h"
#include "CompGeom/DiTOrientedBox.h"

void SSplineToolkitRulesetViewport::Construct(const FArguments& InArgs, TSharedPtr<FSplineToolkitRulesetEditorToolkit> InShowcaseAssetEditor, TSharedPtr<FSplineToolkitRulesetPreviewScene> InPreviewScene)
{
	EditorPtr = InShowcaseAssetEditor;
	PreviewScene = InPreviewScene;
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

SSplineToolkitRulesetViewport::~SSplineToolkitRulesetViewport()
{
	if (TypedViewportClient.IsValid())
	{
		TypedViewportClient->Viewport = nullptr;
	}
}

FString SSplineToolkitRulesetViewport::GetReferencerName() const
{
	return TEXT("SSplineToolkitRulesetViewport");

}

TSharedRef<class SEditorViewport> SSplineToolkitRulesetViewport::GetViewportWidget()
{
	return SharedThis(this);
}
TSharedPtr<FExtender> SSplineToolkitRulesetViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}
void SSplineToolkitRulesetViewport::OnFloatingButtonClicked()
{
	// Nothing
}

void SSplineToolkitRulesetViewport::OnFocusViewportToSelection()
{
	SEditorViewport::OnFocusViewportToSelection();
	
	/* TODO: Replace with PreviewMeshComponent->Bounds */
	const FBoxSphereBounds Bounds = FBoxSphereBounds();
	TypedViewportClient->FocusViewportOnBounds( Bounds, false );
}

TSharedRef<FEditorViewportClient> SSplineToolkitRulesetViewport::MakeEditorViewportClient()
{
	TypedViewportClient = MakeShareable(new FSplineToolkitRulesetViewportClient(SharedThis(this), PreviewScene.ToSharedRef()));
	return TypedViewportClient.ToSharedRef();
}
