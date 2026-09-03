// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineToolkitRulesetEditor/Public/SplineToolkitRulesetEditor.h"

#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "FSplineToolkitModule"

void FSplineToolkitRulesetEditorModule::StartupModule()
{
	RulesetAssetDefinition = MakeShared<FSplineToolkitRulesetTypeActions>();
	FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(RulesetAssetDefinition.ToSharedRef());
}

void FSplineToolkitRulesetEditorModule::ShutdownModule()
{
	if (!FModuleManager::Get().IsModuleLoaded("AssetTools")) return;
	FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(RulesetAssetDefinition.ToSharedRef());
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSplineToolkitRulesetEditorModule, SplineToolkitRulesetEditor)