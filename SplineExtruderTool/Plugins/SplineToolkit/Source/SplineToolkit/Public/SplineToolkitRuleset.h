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

/// An anchor used by rules
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitAnchor
{
public:

	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Type;
	
	// Only set once spawned
	FVector SpawnedWorldLocation = {};
};

/// Modifier threshold type
UENUM(BlueprintType)
enum class ESplineToolkitStretchConnectionType : uint8 {
	Sequential UMETA(DisplayName = "Sequential", ToolTip = "Connect from the first to the next, to the next, etc."),
	Closest UMETA(DisplayName = "Closest", ToolTip = "Connect to the closest point it can find regardless of direction"),
};


// Rule type
UENUM(BlueprintType)
enum class ESplineToolkitRuleType : uint8
{
	INSTANTIATION UMETA(DisplayName = "Intersection"),
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
	float StepPrecision = 10.0f;

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSplineToolkitAnchor> Anchors = {};
	
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
	TObjectPtr<class UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class UMaterialInterface> OverlayMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName StartAnchorType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName EndAnchorType;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float bRollOffset = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D Scale = FVector2D::One();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESplineToolkitStretchConnectionType ConnectionType = ESplineToolkitStretchConnectionType::Sequential;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (EditCondition = "Type == EFillAnchorType::INTERSECT_RULE", EditConditionHides))
	ESplineToolkitRuleType RuleType = ESplineToolkitRuleType::INSTANTIATION;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (EditCondition = "Type == EFillAnchorType::INTERSECT_RULE", EditConditionHides))
	uint8 RuleIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta = (EditCondition = "Type == EFillAnchorType::INTERSECT_RULE", EditConditionHides, ToolTip =
			"Counts from the closest intersection point if there are multiple."))
	uint8 IntersectionIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetDistance = 0.f;
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
