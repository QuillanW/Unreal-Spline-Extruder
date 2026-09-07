// Fill out your copyright notice in the Description page of Project Settings.

#include "Viewport/SplineToolkitRulesetEditorPreviewScene.h"

#include "GameFramework/WorldSettings.h"

FSplineToolkitRulesetPreviewScene::FSplineToolkitRulesetPreviewScene(ConstructionValues CVS, const TSharedRef<FSplineToolkitRulesetEditorToolkit>& EditorToolkit)
	: FAdvancedPreviewScene(CVS)
	, EditorPtr(EditorToolkit)
{
	// Disable killing actors outside of the world
	AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings(true);
	WorldSettings->bEnableWorldBoundsChecks = false;

	//Hide default floor
	SetFloorVisibility(false, false);

	UStaticMesh* PreviewMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EngineMeshes/Cube.Cube"), NULL, LOAD_None, NULL);
	FTransform PreviewMeshTransform (FRotator(0, 0, 0), FVector(0, 0, 0), FVector(1.0f, 1.0f, 1.0f ));
	
	{
		PreviewComponent = NewObject<UStaticMeshComponent>(GetTransientPackage());
		PreviewComponent->SetStaticMesh(PreviewMesh);
		PreviewComponent->bSelectable = true;
		
		AddComponent(PreviewComponent, PreviewMeshTransform);
	}
}

FSplineToolkitRulesetPreviewScene::~FSplineToolkitRulesetPreviewScene()
{
	
}

void FSplineToolkitRulesetPreviewScene::Tick(float InDeltaTime)
{
	FAdvancedPreviewScene::Tick(InDeltaTime);

	
}