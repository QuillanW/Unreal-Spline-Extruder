// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer
#include "SplineToolkitMeshExtruder.h"

#include "Components/SplineComponent.h"


void USplineToolkitMeshExtruder::OnRegister()
{
	Super::OnRegister();

	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;

	if (const AActor* Owner = GetOwner())
	{
		if (!Owner->FindComponentByClass<USplineComponent>())
		{
			UE_LOG(LogTemp, Error, TEXT("Instantiator requires USplineComponent"));
			return;
		}
		this->SplineComponent = Owner->GetComponentByClass<USplineComponent>();
		this->SplineComponent->GetOnSplineChanged().AddLambda([this]()
		{
			RecalculateMesh();
		});
	}
}


void USplineToolkitMeshExtruder::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                               FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (const auto& Sample : this->RmfSamples)
	{
		FMatrix CoordinateMatrix{Sample.Tangent, Sample.Bitangent, Sample.Reference, FVector::ZeroVector};
		DrawDebugCoordinateSystem(GetWorld(), Sample.Position, CoordinateMatrix.Rotator(), 100.f, false, -1, 0, 3.f);
	}
}


void USplineToolkitMeshExtruder::RecalculateMesh()
{
	RecalculateRmfSamples();

	if (!GetStaticMesh() || !GetStaticMesh()->IsValidLowLevelFast())
		return;
}


void USplineToolkitMeshExtruder::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USplineToolkitMeshExtruder, NumRmfSamples))
		RecalculateMesh();
}


void USplineToolkitMeshExtruder::RecalculateRmfSamples()
{
	// Perform simple RMF for now
	this->RmfSamples.Empty();
	this->RmfSamples.Reserve(this->NumRmfSamples);

	// 0th sample is the first tangent
	RMFSample PrevSample = {
		.Position = this->SplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World),
		.Tangent = this->SplineComponent->GetTangentAtSplinePoint(0, ESplineCoordinateSpace::World),
		.Reference = this->SplineComponent->GetUpVectorAtSplinePoint(0, ESplineCoordinateSpace::World),
	};
	PrevSample.Bitangent = PrevSample.Tangent.Cross(PrevSample.Reference);

	this->RmfSamples.Add(PrevSample);

	for (int32 SampleIter = 1; SampleIter < this->NumRmfSamples; ++SampleIter)
	{
		const float Time = SampleIter / static_cast<float>(this->NumRmfSamples - 1);

		const FVector Position = this->SplineComponent->GetLocationAtTime(Time, ESplineCoordinateSpace::World);
		const FVector Tangent = this->SplineComponent->GetTangentAtTime(Time, ESplineCoordinateSpace::World);
		const FRotator Rotation = this->SplineComponent->GetRotationAtTime(Time, ESplineCoordinateSpace::World);

		// Perform the first reflection R_1
		// Algorithm from https://dl.acm.org/doi/epdf/10.1145/1330511.1330513
		// Page 7, Table I
		const FVector Reflection1 = Position - PrevSample.Position;
		const float Reflection1SqrLength = Reflection1.SquaredLength();
		const FVector PrevReferenceLeftHanded = PrevSample.Reference - (2.0f / Reflection1SqrLength) * Reflection1.
			Dot(PrevSample.Reference) * Reflection1;
		const FVector PrevTangentLeftHanded = PrevSample.Tangent - (2.0f / Reflection1SqrLength) * Reflection1.
			Dot(PrevSample.Tangent) * Reflection1;

		const FVector Reflection2 = Tangent - PrevTangentLeftHanded;
		const float Reflection2SqrLength = Reflection2.SquaredLength();
		FVector NewReference = PrevReferenceLeftHanded - (2.0f / Reflection2SqrLength) * Reflection2.Dot(
			PrevReferenceLeftHanded) * Reflection2;

		// Rotate reference vector to spline rotation
		NewReference = Rotation.RotateVector(NewReference);

		const FVector NewBitangent = Tangent.Cross(NewReference);

		auto NewSample = RMFSample{
			.Position = Position,
			.Tangent = Tangent,
			.Bitangent = NewBitangent,
			.Reference = NewReference
		};
		this->RmfSamples.Add(NewSample);
		PrevSample = NewSample;
	}
}
