// Fill out your copyright notice in the Description page of Project Settings.

#include "SplineToolkitRuleset.h"

float GetCurvatureAtDistanceAlongSpline(USplineComponent* Spline, float Distance, float MaxCurvature)
{
	float InputKey = Spline->GetInputKeyValueAtDistanceAlongSpline(Distance);
	const FInterpCurveVector& PositionCurve = Spline->GetSplinePointsPosition();

	FVector FirstDeriv  = PositionCurve.EvalDerivative(InputKey, FVector::ZeroVector);
	FVector SecondDeriv = PositionCurve.EvalSecondDerivative(InputKey, FVector::ZeroVector);

	float SpeedSq = FirstDeriv.SizeSquared();
	if (SpeedSq < SMALL_NUMBER)
	{
		return 0.0f;
	}

	float Speed = FMath::Sqrt(SpeedSq);
	FVector Tangent = FirstDeriv / Speed;

	FVector CrossVec = FVector::CrossProduct(FirstDeriv, SecondDeriv);
	float CurvatureMagnitude = CrossVec.Size() / (Speed * Speed * Speed);

	FVector UpVector = Spline->GetUpVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local);
	FVector RightVector = FVector::CrossProduct(Tangent, UpVector).GetSafeNormal();

	float Side = FVector::DotProduct(SecondDeriv, RightVector);
	float Sign = (Side >= 0.0f) ? 1.0f : -1.0f;

	float Normalized = FMath::Tanh(CurvatureMagnitude / FMath::Max(MaxCurvature, SMALL_NUMBER));
	return Sign * Normalized;
}

#if WITH_EDITOR

void USplineToolkitRuleset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FString Name = PropertyChangedEvent.GetPropertyName().ToString();
	if (Name.Contains("Material"))
		OnReapplyMaterials.Broadcast();
	else
		OnShouldRegenerate.Broadcast();
}
#endif

USplineToolkitRulesetFactory::USplineToolkitRulesetFactory(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
{
	SupportedClass = USplineToolkitRuleset::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USplineToolkitRulesetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
														EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<USplineToolkitRuleset>(InParent, Class, Name, Flags | RF_Transactional);
}