// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Templates/Function.h"
#include "SplineToolkitModifier.generated.h"

// The supported data types used by the modifiers
UENUM(BlueprintType)
enum class EModifierDataType : uint8 {
	Bool UMETA(DisplayName = "Boolean"),
	Float UMETA(DisplayName = "Float"),
	Vec3 UMETA(DisplayName = "3D Vector"),
	Multi UMETA(DisplayName = "Multiple Allowed")
};

// The supported input types
UENUM(BlueprintType)
enum class EModifierInputTypes : uint8 {
	Distance UMETA(DisplayName = "Distance"),
	Time UMETA(DisplayName = "Time (0>1)"),
	Height UMETA(DisplayName = "Height from world 0"),
	Curvature UMETA(DisplayName = "Curvature (0>1)"),
	Roll UMETA(DisplayName = "Roll (-180>180)"),
	Pitch UMETA(DisplayName = "Pitch (-180>180)"),
	Crossing UMETA(DisplayName = "Crossing (Bool)"),
	Split UMETA(DisplayName = "Split (Bool)"),
	Modifier UMETA(DisplayName = "Modifier", ToolTip = "Should only be used for the parameter source"),
	Parameter UMETA(DisplayName = "Parameter", ToolTip = "Should only be used for the parameter source")
};

// The supported operator types
UENUM(BlueprintType)
enum class EModifierOperatorTypes : uint8 {
	GreaterThan UMETA(DisplayName = "Less Than"),
	LessThan UMETA(DisplayName = "Greater Than"),
	Multiply UMETA(DisplayName = "Multiply"),
	Divide UMETA(DisplayName = "Divide"),
	Add UMETA(DisplayName = "Add"),
	Subtract UMETA(DisplayName = "Subtract")
};

// The supported output types
UENUM(BlueprintType)
enum class EModifierOutputTypes : uint8 {
	Enabled UMETA(DisplayName = "Enabled (Bool)"),
	Offset UMETA(DisplayName = "Offset (Vec3)"),
	Rotation UMETA(DisplayName = "Rotation (Vec3)"),
	Scale UMETA(DisplayName = "Scale (Vec3)"),
	Spacing UMETA(DisplayName = "Spacing (Float, Instancing Only)")
};

// Allows passing all supported types within one struct, and extracting only the needed one
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitModifierValue
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EModifierDataType DataType = EModifierDataType::Bool;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBoolParameter = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FloatParameter = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector VectorParameter = FVector::ZeroVector;
};

// An operation to apply to a modifier
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitModifierOperation
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EModifierDataType InputType = EModifierDataType::Bool;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EModifierDataType OutputType = EModifierDataType::Bool;

	TFunction<FSplineToolkitModifierValue(const FSplineToolkitModifierValue& Input, const FSplineToolkitModifierValue& Param)> Apply = [](const FSplineToolkitModifierValue&, const FSplineToolkitModifierValue&){ return FSplineToolkitModifierValue(); };
};

// A modifier used by rules
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitModifier
{
public:
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EModifierInputTypes Input = EModifierInputTypes::Distance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EModifierOperatorTypes Operation = EModifierOperatorTypes::GreaterThan;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSplineToolkitModifierValue Parameter = {};
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SourceModifierIdx = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EModifierInputTypes ParameterSource = {};
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EModifierOutputTypes Output = EModifierOutputTypes::Enabled;
};

// Default modifier types
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitModifierDefaults
{
	GENERATED_BODY()
	
	FSplineToolkitModifierDefaults();
	
	TMap<EModifierInputTypes, TFunction<FSplineToolkitModifierValue(USplineComponent*, float)>> InputFunctions = {};
	TMap<EModifierOperatorTypes, FSplineToolkitModifierOperation> Operators = {};
};