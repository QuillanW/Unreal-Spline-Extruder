// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Engine/DataAsset.h"
#include "SplineToolkitRuleset.generated.h"

// ===============================
//         HELPER FUNCS
// ===============================

// MaxCurvature controls sensitivity, it's the curvature (in 1/cm) that maps to ~1.0.
float GetCurvatureAtDistanceAlongSpline(USplineComponent* Spline, float Distance, float MaxCurvature = 1.0f);

// ===============================
//         GENERIC TYPES
// ===============================

class USplineToolkitRulesetModifierBase;

/// Utility struct
USTRUCT(BlueprintType)
struct FVector2DRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D Start;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D End;
};

/// Utility struct
USTRUCT(BlueprintType)
struct FVectorRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Start;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector End;
};

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


// Rule type
UENUM(BlueprintType)
enum class ESplineToolkitRuleType : uint8
{
	INSTANTIATION UMETA(DisplayName = "Instantiation"),
	EXTRUSION UMETA(DisplayName = "Extrusion"),
	STRETCH UMETA(DisplayName = "Stretch"),
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
	TObjectPtr<class UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UMaterialInterface> OverlayMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StepPrecision = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Enabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Spacing = 100.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator RotationOffset = FRotator::ZeroRotator;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite)
	TArray<TObjectPtr<USplineToolkitRulesetModifierBase>> Modifiers;
};


/// Extrusion rule for its matching component
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitExtrusionRule
{
public:

	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCheckIntersections = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UMaterialInterface> OverlayMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D Scale = FVector2D::One();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float UvScale = 1.0f;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite)
	TArray<TObjectPtr<USplineToolkitRulesetModifierBase>> Modifiers;

	bool operator==(const FSplineToolkitExtrusionRule&) const = default;
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

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite)
	TArray<TObjectPtr<USplineToolkitRulesetModifierBase>> Modifiers;
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
//      RULE OVERRIDE DATA
// ===============================

USTRUCT(BlueprintType)
struct FSplineToolkitInstantiationOverrides
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (ToolTip = "Overrides the spline used for modifier calculation"))
	USplineComponent* ModifierSpline = nullptr;

	// Unreal really wants this to not be a simple TPair. Have it your way then and use two variables. How readable...
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit",
		meta = (ToolTip = "Overrides the extruder to only draw one rule"))
	TOptional<FSplineToolkitInstantiationRule> Rule = NullOpt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit",
		meta = (ToolTip = "Overrides the extruder to only draw one rule"))
	TOptional<int32> RuleIndex = NullOpt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit",
		meta = (ToolTip = "Only used internally."))
	TOptional<FVector2f> Distance = NullOpt;
};

USTRUCT(BlueprintType)
struct FSplineToolkitExtrusionOverrides
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (ToolTip = "Overrides the spline used for modifier calculation"))
	USplineComponent* ModifierSpline = nullptr;

	// Unreal really wants this to not be a simple TPair. Have it your way then and use two variables. How readable...
	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (ToolTip = "Overrides the extruder to only draw one rule"))
	TOptional<FSplineToolkitExtrusionRule> Rule = NullOpt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (ToolTip = "Overrides the extruder to only draw one rule"))
	TOptional<int32> RuleIndex = NullOpt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (ToolTip = "Sets the size range. Overrides any enabled modifiers"))
	TOptional<FVector2DRange> Size = NullOpt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		meta = (ToolTip = "Only used internally."))
	TOptional<FVector2f> Distance = NullOpt;
};

// ===============================
//         FILLING RULES
// ===============================

// The type of anchor to use when filling
UENUM(BlueprintType)
enum class EFillAnchorType : uint8
{
	CUTOUT_START UMETA(DisplayName = "Start of cutout region"),
	CUTOUT_END UMETA(DisplayName = "End of cutout region"),
	INTERSECT_RULE UMETA(DisplayName = "Intersection with a rule"),
};


// Parameters for the start and end config for the connect rule
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitFillingConnectRuleParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EFillAnchorType Type = EFillAnchorType::CUTOUT_START;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 SplineIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESplineToolkitRuleType RuleType = ESplineToolkitRuleType::INSTANTIATION;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 RuleIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (EditCondition = "Type == EFillAnchorType::INTERSECT_RULE", EditConditionHides, ToolTip =
			"Counts from the closest intersection point if there are multiple."))
	uint8 IntersectionIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetDistance = 0.f;

	bool operator==(const FSplineToolkitFillingConnectRuleParams&) const = default;
};


// Rules for connecting a rule between two points
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitFillingConnectRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 SplineIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESplineToolkitRuleType RuleType = ESplineToolkitRuleType::INSTANTIATION;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 RuleIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSplineToolkitFillingConnectRuleParams Start{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSplineToolkitFillingConnectRuleParams End{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumSamples = 32;
};


// Placement rule variant for filling
USTRUCT(BlueprintType)
struct FSplineToolkitFillingPlacementRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EFillAnchorType Type = EFillAnchorType::CUTOUT_START;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset = FVector::ZeroVector;
};


// Rules for filling intersection areas
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitFillingRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AngleMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AngleMax = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (ToolTip = "Set to anything other than -1 to filter only when any rule overlaps a certain amount of times"))
	int32 IntersectCount = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSplineToolkitFillingConnectRule> ConnectRules = {};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSplineToolkitFillingPlacementRule> PlacementRules = {};
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSplineToolkitFillingRules> FillingRules = {};

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	DECLARE_EVENT(USplineToolkitRuleset, FOnShouldRegenerate);


	FOnShouldRegenerate OnShouldRegenerate;

	DECLARE_EVENT(USplineToolkitRuleset, FOnReapplyMaterials);


	FOnReapplyMaterials OnReapplyMaterials;
#endif
};


UCLASS(HideCategories = Object)
class USplineToolkitRulesetFactory : public UFactory
{
	GENERATED_BODY()

public:

	USplineToolkitRulesetFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	                                  UObject* Context, FFeedbackContext* Warn) override;
};
