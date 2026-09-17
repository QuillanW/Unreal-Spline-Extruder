// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SplineToolkitModifier.h"
#include "SplineToolkitRuleset.generated.h"

#pragma region Modifier Operators



#pragma endregion


// ===============================
//         GENERIC TYPES
// ===============================

// RMF Sample type
USTRUCT()
struct FSplineToolkitRmfSample
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position;
	UPROPERTY()
	float Distance;
	UPROPERTY()
	FVector Tangent;   // Front vector
	UPROPERTY()
	FVector Bitangent; // Right vector
	UPROPERTY()
	FVector Reference; // Up vector
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
	TArray<FSplineToolkitModifier> Modifiers;
};

/// Extrusion rule for its matching component
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitExtrusionRule
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumRmfSamples = 128;
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

#if WITH_EDITOR
	virtual void PostEditChangeProperty( FPropertyChangedEvent & PropertyChangedEvent ) override;

	DECLARE_EVENT( UMyDataAsset , FOnChanged );
	FOnChanged OnChanged;
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