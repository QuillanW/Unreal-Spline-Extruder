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
	float Spacing = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset = FVector::ZeroVector;
	
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

#if WITH_EDITOR
	virtual void PostEditChangeProperty( FPropertyChangedEvent & PropertyChangedEvent ) override;

	DECLARE_EVENT( USplineToolkitRuleset , FOnShouldRegenerate );
	FOnShouldRegenerate OnShouldRegenerate;

	DECLARE_EVENT( USplineToolkitRuleset , FOnReapplyMaterials );
	FOnReapplyMaterials OnReapplyMaterials;
#endif
};

UCLASS(HideCategories = Object)
class USplineToolkitRulesetFactory : public UFactory
{
	GENERATED_BODY()

public:
	USplineToolkitRulesetFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass*  Class, UObject*            InParent, FName Name, EObjectFlags Flags,
	                                  UObject* Context, FFeedbackContext* Warn) override;
};