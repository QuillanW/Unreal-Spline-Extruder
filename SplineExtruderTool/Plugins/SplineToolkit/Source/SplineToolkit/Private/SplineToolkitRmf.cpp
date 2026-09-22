#include "SplineToolkitRmf.h"


FSplineToolkitRmfSample SplineToolkit::GetFirstRmfSample(const USplineComponent* SplineComponent)
{
	const float Roll = SplineComponent->GetRollAtSplinePoint(0, ESplineCoordinateSpace::Local);

	FSplineToolkitRmfSample Sample = {
		.Position = SplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local),
		.Distance = 0.0f,
		.Tangent = SplineComponent->GetTangentAtSplinePoint(0, ESplineCoordinateSpace::Local).GetSafeNormal(),
		.Reference = SplineComponent->GetUpVectorAtSplinePoint(0, ESplineCoordinateSpace::Local).GetSafeNormal(),
	};

	Sample.Reference = Sample.Reference.RotateAngleAxis(Roll, Sample.Tangent);
	Sample.Bitangent = Sample.Tangent.Cross(Sample.Reference);

	return Sample;
}


FSplineToolkitRmfSample SplineToolkit::CalculateRmfSampleAtTime(const FSplineToolkitRmfSample& Reference,
                                                                const USplineComponent* SplineComponent, float Time)
{
	const FVector Position = SplineComponent->GetLocationAtTime(Time, ESplineCoordinateSpace::Local);
	const FVector Tangent = SplineComponent->GetTangentAtTime(Time, ESplineCoordinateSpace::Local).
								  GetSafeNormal();
	const float Distance = SplineComponent->GetDistanceAlongSplineAtLocation(
		Position, ESplineCoordinateSpace::Local);

	if (Distance == Reference.Distance)
		return Reference;

	// Perform the first reflection R_1
	// Algorithm from https://dl.acm.org/doi/epdf/10.1145/1330511.1330513
	// Page 7, Table I
	const FVector Reflection1 = Position - Reference.Position;
	const float Reflection1SqrLength = Reflection1.SquaredLength();
	const FVector PrevReferenceLeftHanded = Reference.Reference - (2.0f / Reflection1SqrLength) * Reflection1.
		Dot(Reference.Reference) * Reflection1;
	const FVector PrevTangentLeftHanded = Reference.Tangent - (2.0f / Reflection1SqrLength) * Reflection1.
		Dot(Reference.Tangent) * Reflection1;

	// Perform the second reflection R_2
	const FVector Reflection2 = Tangent - PrevTangentLeftHanded;
	const float Reflection2SqrLength = Reflection2.SquaredLength();
	const FVector NewReference = PrevReferenceLeftHanded - (2.0f / Reflection2SqrLength) * Reflection2.Dot(
		PrevReferenceLeftHanded) * Reflection2;

	const FVector NewBitangent = Tangent.Cross(NewReference);

	// Set roll
	const float Roll = -SplineComponent->GetRollAtTime(Time, ESplineCoordinateSpace::Local);

	auto Sample = FSplineToolkitRmfSample{
		.Position = Position,
		.Distance = Distance,
		.Tangent = Tangent,
		.Bitangent = NewBitangent,
		.Reference = NewReference,
		.Roll = Roll
	};

	Sample.Reference = Sample.Reference.RotateAngleAxis(Roll - Reference.Roll, Sample.Tangent);
	Sample.Bitangent = Sample.Tangent.Cross(Sample.Reference);

	return Sample;
}


FSplineToolkitRmfSample SplineToolkit::CalculateRmfSampleAtDistance(const FSplineToolkitRmfSample& Reference,
	const USplineComponent* SplineComponent, float Distance)
{
	if (Distance == Reference.Distance)
		return Reference;

	const FVector Position = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local);
	const FVector Tangent = SplineComponent->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local).
								  GetSafeNormal();

	// Perform the first reflection R_1
	// Algorithm from https://dl.acm.org/doi/epdf/10.1145/1330511.1330513
	// Page 7, Table I
	const FVector Reflection1 = Position - Reference.Position;
	const float Reflection1SqrLength = Reflection1.SquaredLength();
	const FVector PrevReferenceLeftHanded = Reference.Reference - (2.0f / Reflection1SqrLength) * Reflection1.
		Dot(Reference.Reference) * Reflection1;
	const FVector PrevTangentLeftHanded = Reference.Tangent - (2.0f / Reflection1SqrLength) * Reflection1.
		Dot(Reference.Tangent) * Reflection1;

	// Perform the second reflection R_2
	const FVector Reflection2 = Tangent - PrevTangentLeftHanded;
	const float Reflection2SqrLength = Reflection2.SquaredLength();
	const FVector NewReference = PrevReferenceLeftHanded - (2.0f / Reflection2SqrLength) * Reflection2.Dot(
		PrevReferenceLeftHanded) * Reflection2;

	const FVector NewBitangent = Tangent.Cross(NewReference);

	// Set roll
	const float Roll = -SplineComponent->GetRollAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local);

	auto Sample = FSplineToolkitRmfSample{
		.Position = Position,
		.Distance = Distance,
		.Tangent = Tangent,
		.Bitangent = NewBitangent,
		.Reference = NewReference,
		.Roll = Roll
	};

	Sample.Reference = Sample.Reference.RotateAngleAxis(Roll - Reference.Roll, Sample.Tangent);
	Sample.Bitangent = Sample.Tangent.Cross(Sample.Reference);

	return Sample;
}
