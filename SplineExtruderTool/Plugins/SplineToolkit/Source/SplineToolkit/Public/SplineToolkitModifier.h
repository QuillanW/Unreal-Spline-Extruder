// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
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

USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitModifierOperation
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EModifierDataType InputType = EModifierDataType::Bool;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EModifierDataType OutputType = EModifierDataType::Bool;

	TFunction<FSplineToolkitModifierValue(const FSplineToolkitModifierValue&)> Apply = [](const FSplineToolkitModifierValue&){ return FSplineToolkitModifierValue(); };
};

/// A modifier used by rules
USTRUCT(BlueprintType)
struct SPLINETOOLKIT_API FSplineToolkitModifier
{
public:
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Input = "";
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Operation = "";
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSplineToolkitModifierValue Parameter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Output = "";
};