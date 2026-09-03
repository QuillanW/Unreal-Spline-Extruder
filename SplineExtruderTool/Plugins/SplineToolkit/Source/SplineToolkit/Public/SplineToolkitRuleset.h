// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AssetDefinitionDefault.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SplineToolKitRuleset.generated.h"

// ===============================
//         GENERIC TYPES
// ===============================

/// An anchor used by rules
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitAnchor
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Type;
};

/// Modifier threshold type
UENUM(BlueprintType)
enum class EModifierThresholdOperator : uint8 {
	MIN UMETA(DisplayName = "Minimum Threshold"),
	MAX UMETA(DisplayName = "Maximum Threshold"),
	IF UMETA(DisplayName = "If True"),
	IFNOT UMETA(DisplayName = "If Not True"),
};

/// Modifier modification type
UENUM(BlueprintType)
enum class EModifierModificationType : uint8 {
	MULTIPLY UMETA(DisplayName = "Multiply Modification"),
	DIVIDE UMETA(DisplayName = "Divide Modification"),
	ADD UMETA(DisplayName = "Add Modification"),
	SET UMETA(DisplayName = "Set Modification"),
	INVERT UMETA(DisplayName = "Invert Modification"),
};

/// A modifier used by rules
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitRuleModifier
{
public:
	GENERATED_BODY()
	
	// Input Value
	// Threshold Operator
	// Modify Value
	// Modify Operator
	// Output Value
};


// ===============================
//             RULES
// ===============================

/// Instantiation rule for its matching component
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitInstantiationRule
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* Mesh = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Enabled = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Spacing = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Scale;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSplineToolkitRuleModifier> Modifiers;
};

/// Extrusion rule for its matching component
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitExtrusionRule
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* Mesh = nullptr;
};

/// Stretching rule for its matching component
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitStretchRule
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString StartAnchorType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString EndAnchorType;
};

/// Placement rule for its matching component
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitPlacementRule
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* Mesh = nullptr;
};

// ===============================
//            RULESET
// ===============================

UCLASS(BlueprintType)
class SPLINETOOLKIT_API USplineToolkitRuleset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSplineToolkitInstantiationRule> InstantiationRules = {};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSplineToolkitExtrusionRule> ExtrusionRules = {};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSplineToolkitStretchRule> StretchRules = {};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSplineToolkitPlacementRule> PlacementRules = {};
};

#if WITH_EDITOR


UCLASS()
class UAssetDefinition_SplineToolkitRuleset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return USplineToolkitRuleset::StaticClass(); }
	virtual FText                  GetAssetDisplayName() const override { return FText::FromString("Spline Ruleset"); }

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor::FromSRGBColor(FColor::FromHex("#234287"));
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {FText::FromString("Data")};
		return Categories;
	}
};

#endif

UCLASS(HideCategories = Object)
class USplineToolkitRulesetFactory : public UFactory
{
	GENERATED_BODY()

public:
	USplineToolkitRulesetFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass*  Class, UObject*            InParent, FName Name, EObjectFlags Flags,
	                                  UObject* Context, FFeedbackContext* Warn) override;
};
