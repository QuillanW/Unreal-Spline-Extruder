#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "SplineToolkitRmf.generated.h"

// RMF Sample type
USTRUCT(BlueprintType)
struct FSplineToolkitRmfSample
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Position;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Distance;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Tangent;   // Front vector
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Bitangent; // Right vector
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Reference; // Up vector
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Roll; // This is only needed to calculate relative roll
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Curvature;

	bool operator==(const FSplineToolkitRmfSample&) const = default;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), DisplayName="[Spline Toolkit] RMF Sampler")
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

	UFUNCTION(CallInEditor, BlueprintCallable)
	void Regenerate();

private:

	// Used by the public functions after determining a reference (and a distance)
	FSplineToolkitRmfSample InternalGetSampleAtDistance(const FSplineToolkitRmfSample& Reference, float Distance);

};
