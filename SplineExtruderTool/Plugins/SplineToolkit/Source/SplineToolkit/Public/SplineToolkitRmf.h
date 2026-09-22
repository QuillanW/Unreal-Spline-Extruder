#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "SplineToolkitRmf.generated.h"

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
	UPROPERTY()
	float Roll; // This is only needed to calculate relative roll
};

namespace SplineToolkit
{

SPLINETOOLKIT_API inline FSplineToolkitRmfSample GetFirstRmfSample(const USplineComponent* SplineComponent);

SPLINETOOLKIT_API inline FSplineToolkitRmfSample CalculateRmfSampleAtTime(const FSplineToolkitRmfSample& Reference, const USplineComponent* SplineComponent, float Time);
SPLINETOOLKIT_API inline FSplineToolkitRmfSample CalculateRmfSampleAtDistance(const FSplineToolkitRmfSample& Reference, const USplineComponent* SplineComponent, float Distance);

}
