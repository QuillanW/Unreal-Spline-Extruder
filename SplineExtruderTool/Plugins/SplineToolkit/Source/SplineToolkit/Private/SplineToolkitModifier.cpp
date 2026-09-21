// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer

#include "SplineToolkitModifier.h"

// Why is this not built in on the spline Unreal.?
float GetCurvatureAtDistanceAlongSpline(USplineComponent* Comp, float Dist)
{
	float InputKey = Comp->GetInputKeyValueAtDistanceAlongSpline(Dist);

	const FInterpCurveVector& PositionCurve = Comp->GetSplinePointsPosition();

	FVector FirstDeriv  = PositionCurve.EvalDerivative(InputKey, FVector::ZeroVector);
	FVector SecondDeriv = PositionCurve.EvalSecondDerivative(InputKey, FVector::ZeroVector);

	float SpeedSq = FirstDeriv.SizeSquared();
	if (SpeedSq < SMALL_NUMBER)
		return 0.0f;
	
	float CrossMagnitude = FVector::CrossProduct(FirstDeriv, SecondDeriv).Size();
	float Speed = FMath::Sqrt(SpeedSq);

	return CrossMagnitude / (Speed * Speed * Speed);
}

FSplineToolkitModifierDefaults::FSplineToolkitModifierDefaults()
{
#pragma region Input Functions
	InputFunctions.Add(EModifierInputTypes::Curvature, [](USplineComponent* Comp, float Dist)
	{
		FSplineToolkitModifierValue Result;
		Result.DataType = EModifierDataType::Float;
		Result.FloatParameter = GetCurvatureAtDistanceAlongSpline(Comp, Dist);
		return Result;
	});
	InputFunctions.Add(EModifierInputTypes::Distance, [](USplineComponent*, float Dist)
	{
		FSplineToolkitModifierValue Result;
		Result.DataType = EModifierDataType::Float;
		Result.FloatParameter = Dist;
		return Result;
	});
	InputFunctions.Add(EModifierInputTypes::Crossing, [](USplineComponent*, float)
	{
		FSplineToolkitModifierValue Result;
		Result.DataType = EModifierDataType::Float;
		// TODO: Pls implement Patrick :D
		Result.bBoolParameter = false;
		return Result;
	});
	InputFunctions.Add(EModifierInputTypes::Height, [](USplineComponent* Comp, float Dist)
	{
		FSplineToolkitModifierValue Result;
		Result.DataType = EModifierDataType::Float;
		Result.FloatParameter = Comp->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World).Z;
		return Result;
	});
#pragma endregion 
	
#pragma region Modifier Operators
	Operators.Add(EModifierOperatorTypes::GreaterThan, {
		EModifierDataType::Float,
		EModifierDataType::Bool,
		[](const FSplineToolkitModifierValue& Input, const FSplineToolkitModifierValue& Param)
		{
			FSplineToolkitModifierValue Result = {};
			Result.DataType = EModifierDataType::Bool;
			if (Input.DataType != EModifierDataType::Float) return Result;
			if (Param.DataType != EModifierDataType::Float) return Result;
			Result.bBoolParameter = Input.FloatParameter > Param.FloatParameter;
			return Result;
		}
	});
	
	Operators.Add(EModifierOperatorTypes::LessThan, {
		EModifierDataType::Float,
		EModifierDataType::Bool,
		[](const FSplineToolkitModifierValue& Input, const FSplineToolkitModifierValue& Param)
		{
			FSplineToolkitModifierValue Result = {};
			Result.DataType = EModifierDataType::Bool;
			if (Input.DataType != EModifierDataType::Float) return Result;
			if (Param.DataType != EModifierDataType::Float) return Result;
			Result.bBoolParameter = Input.FloatParameter < Param.FloatParameter;
			return Result;
		}
	});
	
	Operators.Add(EModifierOperatorTypes::Multiply, {
		EModifierDataType::Multi,
		EModifierDataType::Multi,
		[](const FSplineToolkitModifierValue& Input, const FSplineToolkitModifierValue& Param)
		{
			FSplineToolkitModifierValue Result = {};
			Result.DataType = Input.DataType;
			Result.FloatParameter = Input.FloatParameter * Param.FloatParameter;
			Result.VectorParameter = Input.VectorParameter * Param.VectorParameter;
			return Result;
		}
	});
	
	Operators.Add(EModifierOperatorTypes::Divide, {
	EModifierDataType::Multi,
	EModifierDataType::Multi,
	[](const FSplineToolkitModifierValue& Input, const FSplineToolkitModifierValue& Param)
	{
		FSplineToolkitModifierValue Result = {};
		Result.DataType = Input.DataType;
		Result.FloatParameter = Input.FloatParameter / Param.FloatParameter;
		Result.VectorParameter = Input.VectorParameter / Param.VectorParameter;
		return Result;
	}
	});
	
	Operators.Add(EModifierOperatorTypes::Add, {
	EModifierDataType::Multi,
	EModifierDataType::Multi,
	[](const FSplineToolkitModifierValue& Input, const FSplineToolkitModifierValue& Param)
	{
		FSplineToolkitModifierValue Result = {};
		Result.DataType = Input.DataType;
		Result.FloatParameter = Input.FloatParameter + Param.FloatParameter;
		Result.VectorParameter = Input.VectorParameter + Param.VectorParameter;
		return Result;
	}
	});
	
	Operators.Add(EModifierOperatorTypes::Subtract, {
	EModifierDataType::Multi,
	EModifierDataType::Multi,
	[](const FSplineToolkitModifierValue& Input, const FSplineToolkitModifierValue& Param)
	{
		FSplineToolkitModifierValue Result = {};
		Result.DataType = Input.DataType;
		Result.FloatParameter = Input.FloatParameter - Param.FloatParameter;
		Result.VectorParameter = Input.VectorParameter - Param.VectorParameter;
		return Result;
	}
	});

#pragma endregion
}
