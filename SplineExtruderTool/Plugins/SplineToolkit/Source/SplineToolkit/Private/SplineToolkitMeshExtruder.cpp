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

	if (auto* Mesh = GetOwner()->FindComponentByClass<UProceduralMeshComponent>())
	{
		// Reuse the old one
		this->OutMesh = Mesh;
		RecalculateMesh();
	}
	else
	{
		this->OutMesh = NewObject<UProceduralMeshComponent>();
		this->OutMesh->RegisterComponent();
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

	if (!this->InputMesh || !this->InputMesh->IsValidLowLevelFast())
		return;

	ExtractOriginSlice();
	ComputeMesh();
}


void USplineToolkitMeshExtruder::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USplineToolkitMeshExtruder, NumRmfSamples))
		RecalculateMesh();

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USplineToolkitMeshExtruder, InputMesh))
		ExtractOriginSlice();
}


void USplineToolkitMeshExtruder::FExtruderDrawData::Reserve(uint32 NumVertices)
{
	Positions.Reserve(NumVertices);
	Normals.Reserve(NumVertices);
	Tangents.Reserve(NumVertices);
	Uv0.Reserve(NumVertices);
}

void USplineToolkitMeshExtruder::FExtruderDrawData::Init(uint32 NumVertices)
{
	Positions.Init(FVector::ZeroVector, NumVertices);
	Normals.Init(FVector::ZeroVector, NumVertices);
	Tangents.Init(FProcMeshTangent{FVector::ZeroVector, false}, NumVertices);
	Uv0.Init(FVector2D::ZeroVector, NumVertices);
}


void USplineToolkitMeshExtruder::FExtruderDrawData::Insert(const FExtruderDrawData& Other, uint32 Where)
{
	Positions.Insert(Other.Positions.GetData(), Other.Positions.Num(), Where);
	Normals.Insert(Other.Normals.GetData(), Other.Normals.Num(), Where);
	Tangents.Insert(Other.Tangents.GetData(), Other.Tangents.Num(), Where);
	Uv0.Insert(Other.Uv0.GetData(), Other.Uv0.Num(), Where);
}


uint32 USplineToolkitMeshExtruder::FExtruderDrawData::Num() const
{
	return Positions.Num();
}


void USplineToolkitMeshExtruder::RecalculateRmfSamples()
{
	// Perform simple RMF for now
	this->RmfSamples.Empty();
	this->RmfSamples.Reserve(this->NumRmfSamples);

	// 0th sample is the first tangent
	FRmfSample PrevSample = {
		.Position = this->SplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World),
		.Distance = 0.0f,
		.Tangent = this->SplineComponent->GetTangentAtSplinePoint(0, ESplineCoordinateSpace::World).GetSafeNormal(),
		.Reference = this->SplineComponent->GetUpVectorAtSplinePoint(0, ESplineCoordinateSpace::World).GetSafeNormal(),
	};
	PrevSample.Bitangent = PrevSample.Tangent.Cross(PrevSample.Reference);

	this->RmfSamples.Add(PrevSample);

	for (int32 SampleIter = 1; SampleIter < this->NumRmfSamples; ++SampleIter)
	{
		const float Time = SampleIter / static_cast<float>(this->NumRmfSamples - 1);

		const FVector Position = this->SplineComponent->GetLocationAtTime(Time, ESplineCoordinateSpace::World);
		const FVector Tangent = this->SplineComponent->GetTangentAtTime(Time, ESplineCoordinateSpace::World).GetSafeNormal();
		const FRotator Rotation = this->SplineComponent->GetRotationAtTime(Time, ESplineCoordinateSpace::Local);
		const float Distance = this->SplineComponent->GetDistanceAlongSplineAtLocation(
			Position, ESplineCoordinateSpace::World);

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

		auto NewSample = FRmfSample{
			.Position = Position,
			.Distance = Distance,
			.Tangent = Tangent,
			.Bitangent = NewBitangent,
			.Reference = NewReference
		};
		this->RmfSamples.Add(NewSample);
		PrevSample = NewSample;
	}
}


void USplineToolkitMeshExtruder::ExtractOriginSlice()
{
	// Store the origin slice in section 0 of the procedural mesh
	FStaticMeshRenderData* RenderData = this->InputMesh->GetRenderData();
	FStaticMeshLODResources& LOD = RenderData->LODResources[0];

	FPositionVertexBuffer& MeshPositions = LOD.VertexBuffers.PositionVertexBuffer;
	FStaticMeshVertexBuffer& MeshVertexBuffer = LOD.VertexBuffers.StaticMeshVertexBuffer;

	this->OriginSlice = FExtruderDrawData{};
	this->OriginSlice.Reserve(MeshPositions.GetNumVertices());

	// Extract origin slice
	for (uint32 Vertex = 0; Vertex < MeshPositions.GetNumVertices(); ++Vertex)
	{
		FVector VertPos = FVector{MeshPositions.VertexPosition(Vertex)};

		if (FMath::Abs(VertPos.Y) > DOUBLE_KINDA_SMALL_NUMBER)
			continue;

		this->OriginSlice.Positions.Add(VertPos);
		this->OriginSlice.Normals.Add(FVector{MeshVertexBuffer.VertexTangentZ(Vertex)});
		this->OriginSlice.Uv0.Add(FVector2D{MeshVertexBuffer.GetVertexUV(Vertex, 0)});
		this->OriginSlice.Tangents.Add(FProcMeshTangent{
			FVector{FVector3f{MeshVertexBuffer.VertexTangentX(Vertex)}}, false
		});
	}

	// Calculate the centroid
	FVector Centroid = FVector::ZeroVector;
	for (const auto& Position : this->OriginSlice.Positions)
		Centroid += Position;
	Centroid /= this->OriginSlice.Positions.Num();

	// Sort the vertices to make a loop
	TArray<int32> SortedIndices;
	SortedIndices.Reserve(this->OriginSlice.Positions.Num());
	for (int32 i = 0; i < this->OriginSlice.Positions.Num(); ++i)
		SortedIndices.Add(i);

	SortedIndices.Sort([this, Centroid](int32 A, int32 B)
	{
	   const FVector& PositionA = this->OriginSlice.Positions[A];
	   const FVector& PositionB = this->OriginSlice.Positions[B];
	   const float AngleA = FMath::Atan2(PositionA.Z - Centroid.Y, PositionA.X - Centroid.X);
	   const float AngleB = FMath::Atan2(PositionB.Z - Centroid.Y, PositionB.X - Centroid.X);
	   return AngleA < AngleB;
	});

	// Push that data to the local var
	FExtruderDrawData SortedData;
	SortedData.Reserve(this->OriginSlice.Positions.Num());
	for (int32 Idx : SortedIndices)
	{
		SortedData.Positions.Add(this->OriginSlice.Positions[Idx]);
		SortedData.Normals.Add(this->OriginSlice.Normals[Idx]);
		SortedData.Uv0.Add(this->OriginSlice.Uv0[Idx]);
		SortedData.Tangents.Add(this->OriginSlice.Tangents[Idx]);
	}
	this->OriginSlice = MoveTemp(SortedData);
}


void USplineToolkitMeshExtruder::ComputeMesh()
{
	FExtruderDrawData DrawData{};
	DrawData.Init(this->NumRmfSamples * OriginSlice.Num());

	TArray<int32> Indices{};
	Indices.Init(0, (this->NumRmfSamples - 1) * OriginSlice.Num() * 6);

	const float TotalSplineDistance = this->SplineComponent->GetSplineLength();

	uint32 VertexPtr = 0;
	for (const auto& Sample : this->RmfSamples)
	{
		// Instantiate a slice per sample
		DrawData.Insert(OriginSlice, VertexPtr);

		// Create a transform matrix
		FMatrix Rotation{Sample.Bitangent.GetSafeNormal(), Sample.Tangent.GetSafeNormal(), Sample.Reference.GetSafeNormal(), FVector::ZeroVector};
		FTransform Transform;
		Transform.SetComponents(Rotation.ToQuat(), Sample.Position, FVector::OneVector);

		// Transform all vertices with this matrix
		for (uint32 Vertex = 0; Vertex < OriginSlice.Num(); ++Vertex, ++VertexPtr)
		{
			DrawData.Positions[VertexPtr] = Transform.TransformPosition(DrawData.Positions[VertexPtr]);
			DrawData.Normals[VertexPtr] = Transform.TransformVector(DrawData.Normals[VertexPtr]);
			DrawData.Tangents[VertexPtr].TangentX = Transform.TransformVector(DrawData.Tangents[VertexPtr].TangentX);
			// Set UVs to distance / totalDistance
			DrawData.Uv0[VertexPtr].Y = Sample.Distance / TotalSplineDistance;
		}
	}

	// Create triangles
	const int32 SliceCount = OriginSlice.Num();
	const int32 EdgesPerRing = SliceCount;

	uint32 Ptr = 0;
	for (uint32 SampleIdx = 0; SampleIdx < static_cast<uint32>(this->NumRmfSamples) - 1; ++SampleIdx)
	{
		const uint32 CurrentRing = SampleIdx * SliceCount;
		const uint32 NextRing = (SampleIdx + 1) * SliceCount;

		for (int32 V = 0; V < EdgesPerRing; ++V)
		{
			const int32 NextV = (V + 1) % SliceCount;

			const int32 A = CurrentRing + V;
			const int32 B = CurrentRing + NextV;
			const int32 C = NextRing + V;
			const int32 D = NextRing + NextV;

			// winding: CCW front-face as seen from outside the extrusion.
			// Flip to (A, B, C) / (B, D, C) if backfaces show up — depends on your Tangent/Bitangent/Reference handedness.
			Indices[Ptr++] = A;
			Indices[Ptr++] = B;
			Indices[Ptr++] = C;
			Indices[Ptr++] = B;
			Indices[Ptr++] = D;
			Indices[Ptr++] = C;
		}
	}

	this->OutMesh->CreateMeshSection(0, DrawData.Positions, Indices, DrawData.Normals, DrawData.Uv0, {},
	                                 DrawData.Tangents, true);
}
