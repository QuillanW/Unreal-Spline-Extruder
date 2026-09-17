// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer

#include "SplineToolkitEditExtension.h"

bool FSplineToolkitEditExtension::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	const bool bHandled = FSplineComponentVisualizer::VisProxyHandleClick(InViewportClient, VisProxy, Click);

	return bHandled;
}

bool FSplineToolkitEditExtension::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	const bool bHandled = FSplineComponentVisualizer::HandleInputDelta(ViewportClient, Viewport, DeltaTranslate, DeltaRotate, DeltaScale);
	
	UE_LOG(LogTemp, Warning, TEXT("SplineToolkitEditExtension::HandleInputDelta()"));
	
	return bHandled;
}

bool FSplineToolkitEditExtension::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key,
	EInputEvent Event)
{
	const bool bHandled =  FSplineComponentVisualizer::HandleInputKey(ViewportClient, Viewport, Key, Event);
	
	return bHandled;
}
