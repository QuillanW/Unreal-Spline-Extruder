// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

/// An anchor used by rules
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitRuleModifier
{
public:
  GENERATED_BODY()
  
  
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

USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitRuleset
{
public:
  GENERATED_BODY()
  
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  TArray<FSplineToolkitInstantiationRule> InstantiationRules = {};
  
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  TArray<FSplineToolkitExtrusionRule> ExtrusionRules = {};
  
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  TArray<FSplineToolkitStretchRule> StretchRules = {};
  
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  TArray<FSplineToolkitPlacementRule> PlacementRules = {};
};
