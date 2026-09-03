// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "SplineToolkit/Public/SplineToolkitRuleset.h"
#include "Modules/ModuleManager.h"
#include "SplineToolkitRulesetEditor.generated.h"

/// Based on:
/// https://dev.epicgames.com/community/learning/tutorials/vyKB/unreal-engine-creating-a-custom-asset-type-with-its-own-editor-in-c

class FSplineToolkitRulesetTypeActions : public FAssetTypeActions_Base
{
public:
	UClass* GetSupportedClass() const override { return USplineToolkitRuleset::StaticClass(); }
	FText GetName() const override { return INVTEXT("Spline Ruleset"); }
	FColor GetTypeColor() const override { return FColor::FromHex("#234287"); }
	uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};

class FSplineToolkitRulesetEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FSplineToolkitRulesetTypeActions> RulesetAssetDefinition;
};