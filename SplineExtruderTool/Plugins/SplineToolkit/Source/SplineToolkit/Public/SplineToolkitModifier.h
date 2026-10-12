#pragma once

#include "CoreMinimal.h"
#include "SplineToolkitRmf.h"
#include "SplineToolkitRuleset.h"
#include "SplineToolkitModifier.generated.h"

USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitStepContext
{
	GENERATED_BODY()
	
	FSplineToolkitStepContext() = default;
	FSplineToolkitStepContext(USplineComponent* Spline, FSplineToolkitRmfSample Sample);
	
	UPROPERTY(BlueprintReadOnly, Category = "SplineToolkit")
	USplineComponent* SplineComponent = nullptr;
	
	UPROPERTY(BlueprintReadOnly, Category = "SplineToolkit")
	FSplineToolkitRmfSample RMFSample;

	UPROPERTY(BlueprintReadOnly, Category = "SplineToolkit")
	FTransform WorldTransform;
	
	UPROPERTY(BlueprintReadOnly, Category = "SplineToolkit")
	bool bCross;
	
	UPROPERTY(BlueprintReadOnly, Category = "SplineToolkit")
	bool bSplit;
};

UCLASS(Abstract, Blueprintable, EditInlineNew)
class SPLINETOOLKIT_API USplineToolkitRulesetModifierBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "SplineToolkit")
	FSplineToolkitInstantiationRule ModifyInstantiationStep(const FSplineToolkitStepContext& Context, const FSplineToolkitInstantiationRule& InRule) const;
	UFUNCTION(BlueprintNativeEvent, Category = "SplineToolkit")
	FSplineToolkitExtrusionRule ModifyExtrusionStep(const FSplineToolkitStepContext& Context, const FSplineToolkitExtrusionRule& InRule) const;

	virtual FSplineToolkitInstantiationRule ModifyInstantiationStep_Implementation(const FSplineToolkitStepContext& Context, const FSplineToolkitInstantiationRule& InRule) const
	{
		return InRule;
	}
	
	virtual FSplineToolkitExtrusionRule ModifyExtrusionStep_Implementation(const FSplineToolkitStepContext& Context, const FSplineToolkitExtrusionRule& InRule) const
	{
		return InRule;
	}
};