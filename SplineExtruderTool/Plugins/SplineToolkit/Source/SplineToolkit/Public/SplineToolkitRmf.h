#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "SplineToolkitRmf.generated.h"

// RMF Sample type
USTRUCT(BlueprintType)
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

	bool operator==(const FSplineToolkitRmfSample&) const = default;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SPLINETOOLKIT_API USplineToolkitRmfSampler : public UActorComponent
{
	GENERATED_BODY()

public:

	void OnRegister() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	int32 NumRmfSamples = 128;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	TArray<FSplineToolkitRmfSample> Samples;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	USplineComponent* SplineComponent = nullptr;

	FSplineToolkitRmfSample GetSampleAtTime(float Time);
	FSplineToolkitRmfSample GetSampleAtDistance(float Distance);

	FSplineToolkitRmfSample GetNextSampleFromDistance(float Distance);

private:

	void Regenerate();

	// Used by the public functions after determining a reference (and a distance)
	FSplineToolkitRmfSample InternalGetSampleAtDistance(const FSplineToolkitRmfSample& Reference, float Distance);

};
