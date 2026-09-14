// Fill out your copyright notice in the Description page of Project Settings.

#include "SplineToolkitRuleset.h"

#if WITH_EDITOR
void USplineToolkitRuleset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnChanged.Broadcast();
}
#endif

USplineToolkitRulesetFactory::USplineToolkitRulesetFactory(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
{
	SupportedClass = USplineToolkitRuleset::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USplineToolkitRulesetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
														EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<USplineToolkitRuleset>(InParent, Class, Name, Flags | RF_Transactional);
}