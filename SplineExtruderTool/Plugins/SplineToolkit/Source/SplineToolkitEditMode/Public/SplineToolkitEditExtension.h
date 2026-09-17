// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer

#pragma once

#include "Editor/ComponentVisualizers/Public/SplineComponentVisualizer.h"

// Visualizer extension to handle connections
class FSplineToolkitEditExtension : public FSplineComponentVisualizer
{
public:
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	
protected:
	
};
