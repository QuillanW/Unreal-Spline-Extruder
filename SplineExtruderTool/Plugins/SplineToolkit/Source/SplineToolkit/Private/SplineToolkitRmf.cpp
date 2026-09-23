#include "SplineToolkitRmf.h"

int32 GSplineToolkitShowRmfSamples = 0;
static FAutoConsoleVariableRef CVarShowExtruderRmfSamples(
	TEXT("stk.RMF.ShowSamples"),
	GSplineToolkitShowRmfSamples,
	TEXT("Shows the RMF samples on all splines with the sampler component."));


void USplineToolkitRmfSampler::OnRegister()
{
	Super::OnRegister();

	if (const AActor* Owner = GetOwner())
	{
		if (!Owner->FindComponentByClass<USplineComponent>())
		{
			UE_LOG(LogTemp, Error, TEXT("RMF Sampler requires USplineComponent"));
			return;
		}
		this->SplineComponent = Owner->GetComponentByClass<USplineComponent>();
		this->SplineComponent->GetOnSplineChanged().AddLambda([this]()
		{
			Regenerate();
		});
	}
}


void USplineToolkitRmfSampler::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GSplineToolkitShowRmfSamples)
	{
		for (const auto& Sample : Samples)
		{
			FMatrix CoordinateMatrix{
				Sample.Bitangent.GetSafeNormal(), Sample.Tangent.GetSafeNormal(), Sample.Reference.GetSafeNormal(),
				FVector::ZeroVector
			};
			DrawDebugCoordinateSystem(GetWorld(), Sample.Position + GetOwner()->GetActorLocation(),
			                          CoordinateMatrix.Rotator(), 100.f, false, -1, 0,
			                          3.f);
		}
	}
}


FSplineToolkitRmfSample USplineToolkitRmfSampler::GetSampleAtTime(float Time)
{
	if (!this->SplineComponent)
		return {};
	const float Distance = this->SplineComponent->GetDistanceAlongSplineAtLocation(
		this->SplineComponent->GetLocationAtTime(Time, ESplineCoordinateSpace::Local), ESplineCoordinateSpace::Local);

	return GetSampleAtDistance(Distance);
}


FSplineToolkitRmfSample USplineToolkitRmfSampler::GetSampleAtDistance(float Distance)
{
	if (this->Samples.IsEmpty())
		return {};

	// Find the next sample based on distance
	const auto* BestSample = &this->Samples[0];
	for (const auto& Sample : this->Samples)
	{
		if (Sample.Distance > BestSample->Distance && Sample.Distance < Distance)
			BestSample = &Sample;
	}

	if (BestSample->Distance == Distance)
		return *BestSample;
	return InternalGetSampleAtDistance(*BestSample, Distance);
}


FSplineToolkitRmfSample USplineToolkitRmfSampler::GetNextSampleFromDistance(float Distance)
{
	if (this->Samples.IsEmpty())
		return {};

	// Find the next sample based on distance
	const auto* BestSample = &this->Samples.Last();
	for (const auto& Sample : this->Samples)
	{
		if (Sample.Distance < BestSample->Distance && Sample.Distance > Distance)
			BestSample = &Sample;
	}
	return *BestSample;
}


void USplineToolkitRmfSampler::Regenerate()
{
	if (!this->SplineComponent)
		return;

	this->Samples.SetNum(this->NumRmfSamples);

	// Get the first sample
	const float StartRoll = -SplineComponent->GetRollAtSplinePoint(0, ESplineCoordinateSpace::Local);

	FSplineToolkitRmfSample PrevSample = {
		.Position = SplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local),
		.Distance = 0.0f,
		.Tangent = SplineComponent->GetTangentAtSplinePoint(0, ESplineCoordinateSpace::Local).GetSafeNormal(),
		.Reference = SplineComponent->GetUpVectorAtSplinePoint(0, ESplineCoordinateSpace::Local).GetSafeNormal(),
		.Roll = StartRoll
	};

	PrevSample.Bitangent = PrevSample.Tangent.Cross(PrevSample.Reference);

	this->Samples[0] = PrevSample;

	for (int32 SampleIter = 1; SampleIter < this->NumRmfSamples; ++SampleIter)
	{
		const float Distance = (SampleIter / static_cast<float>(this->NumRmfSamples - 1)) * this->SplineComponent->
			GetSplineLength();

		const auto Sample = InternalGetSampleAtDistance(PrevSample, Distance);
		this->Samples[SampleIter] = Sample;
		PrevSample = Sample;
	}
}


FSplineToolkitRmfSample USplineToolkitRmfSampler::InternalGetSampleAtDistance(const FSplineToolkitRmfSample& Reference,
                                                                              float Distance)
{
	const FVector Position = this->SplineComponent->GetLocationAtDistanceAlongSpline(
		Distance, ESplineCoordinateSpace::Local);

	const FVector Tangent = this->SplineComponent->GetTangentAtDistanceAlongSpline(
		                              Distance, ESplineCoordinateSpace::Local).
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
