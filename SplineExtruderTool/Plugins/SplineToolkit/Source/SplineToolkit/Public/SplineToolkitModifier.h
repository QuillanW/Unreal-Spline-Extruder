// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer

#pragma once

#include "CoreMinimal.h"
#include "SplineToolkitModifier.generated.h"

// The supported data types used by the modifiers
UENUM(BlueprintType)
enum class EModifierDataType : uint8 {
	Bool UMETA(DisplayName = "Boolean"),
	Float UMETA(DisplayName = "Float"),
	Vec3 UMETA(DisplayName = "3D Vector")
};

// Allows passing all supported types within one struct, and extracting only the needed one
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitModifierValue
{
	EModifierDataType DataType = EModifierDataType::Bool;
	
	bool bBoolParameter = false;
	float FloatParameter = 0.0f;
	FVector VectorParameter = FVector::ZeroVector;
};

// An input type for the modifiers
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitModifierInput
{
	FString Name = "Input";
	EModifierDataType DataType = EModifierDataType::Bool;
};

USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitModifierOutput
{
	FString Name = "Output";
	EModifierDataType DataType = EModifierDataType::Bool;
};

USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitModifierOperation
{
	FString Name = "Operation";
	EModifierDataType InputType = EModifierDataType::Bool;
	EModifierDataType OutputType = EModifierDataType::Bool;
	TFunction<FSplineToolkitModifierValue(const FSplineToolkitModifierValue&)> Apply = [](const FSplineToolkitModifierValue&){ return {}; };
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
struct SPLINETOOLKIT_API FSplineToolkitModifier
{
public:
	GENERATED_BODY()
	
	
	
	// Input Value
	// Threshold Operator
	// Modify Value
	// Modify Operator
	// Output Value
};