// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SplineComponent.h"
#include "SplineToolkitConnector.generated.h"


USTRUCT()
struct FSplineConnection
{
	GENERATED_BODY()

	// The spline that is being attached to from this spline
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USplineComponent> ToSpline;

	// Whether it is connected from the end or beginning of this spline
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bFromEnd = false;

	// Whether it is connected to the end or beginning of the other spline
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bToEnd = false;

	// Whether this connection is enabled within the group of connections
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnabled = true;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SPLINETOOLKIT_API USplineToolkitConnector : public UActorComponent
{
	GENERATED_BODY()
protected:
	virtual void OnRegister() override;
	
public:

	// Attach a new spline to this spline
	void Attach(const FSplineConnection& Connection);

	// Automatically find the 2 closest points between the attached spline and the given target spline
	void AutoAttach(USplineComponent* Target);
	
	// Validate the connections on this component
	// Removes any invalid connections and make sure the options are followed correctly
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Spline Toolkit")
	void Validate();
	
	// Update splines attached to this one to be re aligned
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Spline Toolkit")
	void ReAttach();

	// Whether multiple connections are allowed to be enabled in one direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	bool bAllowMultipleEnabled = false;
	
	// Whether multiple connections are allowed to be enabled in one direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	bool bAutoUpdate = false;

	// A list of the connections to this spline
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Toolkit")
	TArray<FSplineConnection> Connections;
	
	// The spline that is attached to this object
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline Toolkit")
	USplineComponent* SplineComponent = nullptr;
};
