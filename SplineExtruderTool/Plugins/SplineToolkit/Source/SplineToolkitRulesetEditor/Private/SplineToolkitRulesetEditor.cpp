// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineToolkitRulesetEditor/Public/SplineToolkitRulesetEditor.h"

#include "AssetToolsModule.h"
#include "Viewport/SplineToolkitRulesetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "FSplineToolkitModule"

void FSplineToolkitRulesetTypeActions::OpenAssetEditor(
    const TArray<UObject *> &InObjects,
    TSharedPtr<class IToolkitHost> EditWithinLevelEditor) {
	
	const EToolkitMode::Type Mode =
		EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (USplineToolkitRuleset* PoseSearchDb = Cast<USplineToolkitRuleset>(*ObjIt))
		{
			const TSharedRef<FSplineToolkitRulesetEditorToolkit> NewEditor(new FSplineToolkitRulesetEditorToolkit());
			NewEditor->InitAssetEditor(Mode, EditWithinLevelEditor, PoseSearchDb);
		}
	}
}
void FSplineToolkitRulesetEditorModule::StartupModule()
{
	RulesetAssetDefinition = MakeShared<FSplineToolkitRulesetTypeActions>();
	FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(RulesetAssetDefinition.ToSharedRef());
	
	ToolbarExtensibilityManager = MakeShareable(new FExtensibilityManager);
}

void FSplineToolkitRulesetEditorModule::ShutdownModule()
{
	if (!FModuleManager::Get().IsModuleLoaded("AssetTools")) return;
	FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(RulesetAssetDefinition.ToSharedRef());
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSplineToolkitRulesetEditorModule, SplineToolkitRulesetEditor)