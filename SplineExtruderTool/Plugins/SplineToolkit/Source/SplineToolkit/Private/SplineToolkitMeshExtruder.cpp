// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer
#include "SplineToolkitMeshExtruder.h"

#include "Components/SplineComponent.h"

int32 GSplineToolkitShowExtruderRmfSamples = 0;
static FAutoConsoleVariableRef CVarShowExtruderRmfSamples(
	TEXT("stk.Extruder.ShowRmfSamples"),
	GSplineToolkitShowExtruderRmfSamples,
	TEXT("Shows the RMF samples used by the extruder to subdivide and construct a spline mesh."));

int32 GSplineToolkitShowExtruderIndices = 0;
static FAutoConsoleVariableRef CVarShowExtruderIndices(
	TEXT("stk.Extruder.ShowIndices"),
	GSplineToolkitShowExtruderIndices,
	TEXT(
		"Shows the indices of the start cap. This is useful when debugging the code in the plugin itself. Only works in PIE/Runtime."));


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
			if (bUpdateOnSplineChange)
				Regenerate();
		});
	}

	if (this->Ruleset)
	{
		this->Ruleset->OnChanged.AddLambda([this]()
		{
			Regenerate();
		});
	}

	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
	{
		for (const auto& Data : this->OutMeshes)
		{
			if (IsValid(Data.MeshActor))
			{
				Data.MeshActor->AttachToActor(GetOwner(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				UE_LOG(LogTemp, Warning, TEXT("Owner root: %s, MeshActor root: %s"),
				       *GetNameSafe(GetOwner()->GetRootComponent()),
				       *GetNameSafe(Data.MeshActor->GetRootComponent()));
			}
		}
	});
}


void USplineToolkitMeshExtruder::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                               FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GSplineToolkitShowExtruderRmfSamples)
	{
		for (const auto& Data : this->OutMeshes)
		{
			for (const auto& Sample : Data.RmfSamples)
			{
				FMatrix CoordinateMatrix{
					Sample.Bitangent.GetSafeNormal(), Sample.Tangent.GetSafeNormal(), Sample.Reference.GetSafeNormal(),
					FVector::ZeroVector
				};
				DrawDebugCoordinateSystem(GetWorld(), Sample.Position + Data.MeshActor->GetActorLocation(),
				                          CoordinateMatrix.Rotator(), 100.f, false, -1, 0,
				                          3.f);
			}
		}
	}

	if (GSplineToolkitShowExtruderIndices)
	{
		for (const auto& Data : this->OutMeshes)
		{
			if (Data.RmfSamples.IsEmpty())
				continue;

			// Create a transform matrix
			const auto& Sample = Data.RmfSamples[0];
			FMatrix Rotation{
				Sample.Bitangent.GetSafeNormal(), Sample.Tangent.GetSafeNormal(), Sample.Reference.GetSafeNormal(),
				FVector::ZeroVector
			};
			FTransform Transform;
			Transform.SetComponents(Rotation.ToQuat(),
			                        Sample.Position + GetOwner()->GetActorLocation(), FVector::OneVector);

			uint32 Index = 0;
			for (const auto& Pos : Data.OriginSlice.Positions)
				DrawDebugString(GetWorld(), Transform.TransformPosition(Pos), FString::FromInt(Index++));
		}
	}

	if (this->bRegenerate)
		RegenerateInternal();
}


void USplineToolkitMeshExtruder::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	Clear();
}


AActor* USplineToolkitMeshExtruder::GetAssociatedActorOfRule(const FSplineToolkitExtrusionRule& Rule) const
{
	const auto Idx = this->Ruleset->ExtrusionRules.Find(Rule);
	return Idx == INDEX_NONE ? nullptr : this->OutMeshes[Idx].MeshActor;
}


void USplineToolkitMeshExtruder::Regenerate()
{
	this->bRegenerate = true;
}


void USplineToolkitMeshExtruder::RegenerateInternal()
{
	this->bRegenerate = false;

	ClearConservative();

	if (!this->Ruleset->IsValidLowLevelFast())
		return;

	// This is a degenerate spline
	if (SplineComponent->GetNumberOfSplinePoints() < 2)
		return;

	uint32 Ptr = 0;
	for (const auto& Rule : this->Ruleset->ExtrusionRules)
	{
		auto& Data = this->OutMeshes[Ptr++];

		RecalculateRmfSamples(Rule.NumRmfSamples, Data);
		ExtractOriginSlice(Rule.Mesh, Data);

		if (auto* MeshComponent = Data.MeshActor->FindComponentByClass<UProceduralMeshComponent>();
			!Data.OriginSlice.IsEmpty() && MeshComponent)
		{
			ComputeMesh(Rule, MeshComponent, Data);
		}
	}
}


void USplineToolkitMeshExtruder::Clear()
{
	for (const auto& Data : this->OutMeshes)
	{
		if (Data.MeshActor.IsResolved() && Data.MeshActor->IsValidLowLevelFast())
			Data.MeshActor->Destroy();
	}
	this->OutMeshes.Empty();
}


void USplineToolkitMeshExtruder::ClearConservative()
{
	auto InitMeshActor = [this, Counter = 0](TObjectPtr<AActor>& Out,
	                                         TObjectPtr<UProceduralMeshComponent>& OutMesh) mutable
	{
		Out = GetWorld()->SpawnActor<AActor>();
#if WITH_EDITOR
		Out->SetActorLabel(TEXT("SplineExtruderInstance") + FString::FromInt(Counter++));
#endif
		OutMesh = NewObject<UProceduralMeshComponent>(Out, NAME_None, RF_Transactional);
		OutMesh->CreationMethod = EComponentCreationMethod::Instance;
		OutMesh->SetupAttachment(Out->GetRootComponent());
		OutMesh->RegisterComponent();
		Out->AddInstanceComponent(OutMesh);
		Out->SetRootComponent(OutMesh);
		Out->AttachToActor(GetOwner(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	};

	// First, clear all data in the lists
	for (auto& Data : this->OutMeshes)
	{
		Data = FSplineToolkitExtruderMeshData{
			.MeshActor = Data.MeshActor
		};

		// Recreate an actor when the one it has currently is not valid
		if (!IsValid(Data.MeshActor))
			InitMeshActor(Data.MeshActor, Data.ProceduralMeshComponent);

		if (!Data.MeshActor->FindComponentByClass<UProceduralMeshComponent>())
			Data.MeshActor->AddComponentByClass(UProceduralMeshComponent::StaticClass(), false, FTransform::Identity,
			                                    false);
	}

	// Skip on garbage data
	if (!this->Ruleset->IsValidLowLevelFast())
		return;

	// Then, shrink or grow to fit
	if (this->OutMeshes.Num() >= this->Ruleset->ExtrusionRules.Num())
	{
		for (int32 I = this->OutMeshes.Num() - 1; I >= this->Ruleset->ExtrusionRules.Num(); --I)
		{
			this->OutMeshes[I].MeshActor->Destroy();
			this->OutMeshes.RemoveAt(I);
		}
	}
	else
	{
		for (int32 I = this->OutMeshes.Num(); I < this->Ruleset->ExtrusionRules.Num(); ++I)
		{
			auto& Data = this->OutMeshes.Emplace_GetRef();
			InitMeshActor(Data.MeshActor, Data.ProceduralMeshComponent);
		}
	}
}


void FSplineToolkitExtruderDrawData::Reserve(uint32 NumVertices)
{
	this->Positions.Reserve(NumVertices);
	this->Normals.Reserve(NumVertices);
	this->Tangents.Reserve(NumVertices);
	this->Uv0.Reserve(NumVertices);
}


void FSplineToolkitExtruderDrawData::Init(uint32 NumVertices)
{
	this->Positions.Init(FVector::ZeroVector, NumVertices);
	this->Normals.Init(FVector::ZeroVector, NumVertices);
	this->Tangents.Init(FProcMeshTangent{FVector::ZeroVector, false}, NumVertices);
	this->Uv0.Init(FVector2D::ZeroVector, NumVertices);
}


void FSplineToolkitExtruderDrawData::Insert(const FSplineToolkitExtruderDrawData& Other, uint32 Where)
{
	this->Positions.Insert(Other.Positions.GetData(), Other.Positions.Num(), Where);
	this->Normals.Insert(Other.Normals.GetData(), Other.Normals.Num(), Where);
	this->Tangents.Insert(Other.Tangents.GetData(), Other.Tangents.Num(), Where);
	this->Uv0.Insert(Other.Uv0.GetData(), Other.Uv0.Num(), Where);
}


int32 FSplineToolkitExtruderDrawData::Num() const
{
	return this->Positions.Num();
}


bool FSplineToolkitExtruderDrawData::IsEmpty() const
{
	return this->Positions.IsEmpty();
}


void USplineToolkitMeshExtruder::RecalculateRmfSamples(int32 NumRmfSamples, FSplineToolkitExtruderMeshData& Data) const
{
	// Perform simple RMF for now
	Data.RmfSamples.Empty();
	Data.RmfSamples.Reserve(NumRmfSamples);

	// 0th sample is the first tangent
	FSplineToolkitRmfSample PrevSample = {
		.Position = this->SplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local),
		.Distance = 0.0f,
		.Tangent = this->SplineComponent->GetTangentAtSplinePoint(0, ESplineCoordinateSpace::Local).GetSafeNormal(),
		.Reference = this->SplineComponent->GetUpVectorAtSplinePoint(0, ESplineCoordinateSpace::Local).GetSafeNormal(),
	};
	PrevSample.Bitangent = PrevSample.Tangent.Cross(PrevSample.Reference);

	Data.RmfSamples.Add(PrevSample);

	for (int32 SampleIter = 1; SampleIter < NumRmfSamples; ++SampleIter)
	{
		const float Time = SampleIter / static_cast<float>(NumRmfSamples - 1);

		const FVector Position = this->SplineComponent->GetLocationAtTime(Time, ESplineCoordinateSpace::Local);
		const FVector Tangent = this->SplineComponent->GetTangentAtTime(Time, ESplineCoordinateSpace::Local).
		                              GetSafeNormal();
		const float Distance = this->SplineComponent->GetDistanceAlongSplineAtLocation(
			Position, ESplineCoordinateSpace::Local);

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
		const FVector NewReference = PrevReferenceLeftHanded - (2.0f / Reflection2SqrLength) * Reflection2.Dot(
			PrevReferenceLeftHanded) * Reflection2;

		const FVector NewBitangent = Tangent.Cross(NewReference);

		auto NewSample = FSplineToolkitRmfSample{
			.Position = Position,
			.Distance = Distance,
			.Tangent = Tangent,
			.Bitangent = NewBitangent,
			.Reference = NewReference
		};
		Data.RmfSamples.Add(NewSample);
		PrevSample = NewSample;
	}

	// Apply roll
	for (auto& Sample : Data.RmfSamples)
	{
		const float Roll = -SplineComponent->GetRollAtDistanceAlongSpline(
			Sample.Distance, ESplineCoordinateSpace::Local);

		Sample.Reference = Sample.Reference.RotateAngleAxis(Roll, Sample.Tangent);
		Sample.Bitangent = Sample.Tangent.Cross(Sample.Reference);
	}
}


void USplineToolkitMeshExtruder::ExtractOriginSlice(UStaticMesh* InputMesh, FSplineToolkitExtruderMeshData& Data) const
{
	if (!InputMesh->IsValidLowLevelFast())
		return;

	Data.LinkedMesh = InputMesh;

	// Store the origin slice in section 0 of the procedural mesh
	FStaticMeshRenderData* RenderData = InputMesh->GetRenderData();
	FStaticMeshLODResources& LOD = RenderData->LODResources[0];

	FPositionVertexBuffer& MeshPositions = LOD.VertexBuffers.PositionVertexBuffer;
	FStaticMeshVertexBuffer& MeshVertexBuffer = LOD.VertexBuffers.StaticMeshVertexBuffer;
	FRawStaticIndexBuffer& Indices = LOD.IndexBuffer;

	Data.OriginSlice = FSplineToolkitExtruderDrawData{};
	Data.OriginSlice.Reserve(MeshPositions.GetNumVertices());

	// Stores what vertices are used for the
	TArray<int32> SliceVertices;

	// Extract origin slice
	for (uint32 Vertex = 0; Vertex < MeshPositions.GetNumVertices(); ++Vertex)
	{
		FVector VertPos = FVector{MeshPositions.VertexPosition(Vertex)};

		if (FMath::Abs(VertPos.Y) > DOUBLE_KINDA_SMALL_NUMBER)
			continue;

		SliceVertices.Add(Vertex);

		Data.OriginSlice.Positions.Add(VertPos);
		Data.OriginSlice.Normals.Add(FVector{MeshVertexBuffer.VertexTangentZ(Vertex)});
		Data.OriginSlice.Uv0.Add(FVector2D{MeshVertexBuffer.GetVertexUV(Vertex, 0)});
		Data.OriginSlice.Tangents.Add(FProcMeshTangent{
			FVector{FVector3f{MeshVertexBuffer.VertexTangentX(Vertex)}}, false
		});
	}

	// First remove duplicates and merge into one translation table

	// This one is used to generate the loop
	TMap<int32, TArray<int32>> TranslationTable{};
	// This one is used to generate sorted geometry
	TMap<int32, int32> BuildTranslationTable{};
	{
		TArray<FVector> SeenPositions{};
		for (int32 I = 0; I < Data.OriginSlice.Num(); ++I)
		{
			if (auto FoundIndex = SeenPositions.Find(Data.OriginSlice.Positions[I]); FoundIndex == INDEX_NONE)
			{
				// Not seen yet, no problem
				SeenPositions.Add(Data.OriginSlice.Positions[I]);
				TranslationTable.Add(SeenPositions.Num() - 1, TArray{SliceVertices[I]});
				BuildTranslationTable.Add(SeenPositions.Num() - 1, I);
			}
			else
			{
				// Seen, add another translation entry
				TranslationTable[FoundIndex].Add(SliceVertices[I]);
			}
		}
	}

	// Sort the vertices to make a loop
	TArray<int32> SortedIndices = ReorderToLoop(Indices, Data.OriginSlice.Positions, TranslationTable);

	// Push that data to the local var
	FSplineToolkitExtruderDrawData SortedData{};
	SortedData.Init(SortedIndices.Num());

	int32 Ptr = 0;
	for (const int32 Idx : SortedIndices)
	{
		SortedData.Positions[Ptr] = Data.OriginSlice.Positions[BuildTranslationTable[Idx]];
		SortedData.Normals[Ptr] = Data.OriginSlice.Normals[BuildTranslationTable[Idx]];
		SortedData.Uv0[Ptr] = Data.OriginSlice.Uv0[BuildTranslationTable[Idx]];
		SortedData.Tangents[Ptr++] = Data.OriginSlice.Tangents[BuildTranslationTable[Idx]];
	}
	Data.OriginSlice = MoveTemp(SortedData);
}


// Generated by Claude Sonnet 5 with some tweaks to make it work for my application
TArray<int32> USplineToolkitMeshExtruder::ComputeEndCap(const FSplineToolkitExtruderMeshData& Data, int16 IndexOffset,
                                                        bool InvertOrdering)
{
	TArray<int32> Indices{};

	TArray<TPair<int32, FVector2D>> Points{};
	Points.Reserve(Data.OriginSlice.Num());
	for (int32 I = 0; I < Data.OriginSlice.Num(); ++I)
	{
		const FVector& P = Data.OriginSlice.Positions[I];
		Points.Emplace(I, FVector2D(P.X, P.Z));
	}

	auto Cross2D = [](const FVector2D& A, const FVector2D& B) -> float
	{
		return A.X * B.Y - A.Y * B.X;
	};

	auto IsConvex = [&Points, &Cross2D](int32 I) -> bool
	{
		const int32 Prev = (I == 0) ? Points.Num() - 1 : I - 1;
		const int32 Next = (I == Points.Num() - 1) ? 0 : I + 1;
		const FVector2D V1 = Points[I].Value - Points[Prev].Value;
		const FVector2D V2 = Points[Next].Value - Points[I].Value;
		return Cross2D(V1, V2) > 0.0f;
	};

	auto PointInTriangle = [&Cross2D](const FVector2D& P, const FVector2D& A, const FVector2D& B,
	                                  const FVector2D& C) -> bool
	{
		constexpr float Epsilon = 1e-5f;
		const float D1 = Cross2D(B - A, P - A);
		const float D2 = Cross2D(C - B, P - B);
		const float D3 = Cross2D(A - C, P - C);
		const bool HasNeg = (D1 < -Epsilon) || (D2 < -Epsilon) || (D3 < -Epsilon);
		const bool HasPos = (D1 > Epsilon) || (D2 > Epsilon) || (D3 > Epsilon);
		return !(HasNeg && HasPos) && (HasNeg || HasPos);
	};

	auto IsEar = [&Points, &IsConvex, &PointInTriangle](int32 I) -> bool
	{
		if (!IsConvex(I))
			return false;

		const int32 Prev = (I == 0) ? Points.Num() - 1 : I - 1;
		const int32 Next = (I == Points.Num() - 1) ? 0 : I + 1;
		const FVector2D& A = Points[Prev].Value;
		const FVector2D& B = Points[I].Value;
		const FVector2D& C = Points[Next].Value;

		for (int32 J = 0; J < Points.Num(); ++J)
		{
			if (J == Prev || J == I || J == Next)
				continue;
			if (PointInTriangle(Points[J].Value, A, B, C))
				return false;
		}
		return true;
	};

	while (Points.Num() > 2)
	{
		int32 EarIndex = INDEX_NONE;
		for (int32 I = 0; I < Points.Num(); ++I)
		{
			if (IsEar(I))
			{
				EarIndex = I;
				break;
			}
		}

		if (EarIndex == INDEX_NONE)
			break;

		const int32 Prev = (EarIndex == 0) ? Points.Num() - 1 : EarIndex - 1;
		const int32 Next = (EarIndex == Points.Num() - 1) ? 0 : EarIndex + 1;
		const int32 Idx0 = Points[Prev].Key;
		const int32 Idx1 = Points[EarIndex].Key;
		const int32 Idx2 = Points[Next].Key;

		if (InvertOrdering)
			Indices.Append(TArray{Idx2 + IndexOffset, Idx1 + IndexOffset, Idx0 + IndexOffset});
		else
			Indices.Append(TArray{Idx0 + IndexOffset, Idx1 + IndexOffset, Idx2 + IndexOffset});

		Points.RemoveAt(EarIndex);
	}

	return Indices;
}


TArray<int32> USplineToolkitMeshExtruder::ReorderToLoop(const FRawStaticIndexBuffer& GeometryIndexBuffer,
                                                        const TArray<FVector>& Positions,
                                                        const TMap<int32, TArray<int32>>& UsedIndices)
{
	// Gather all vertices that are connected to one another
	TMap<int32, TArray<int32>> Adjacency;

	for (int32 Ptr = 0; Ptr < GeometryIndexBuffer.GetNumIndices(); Ptr += 3)
	{
		const int32 A = GeometryIndexBuffer.GetIndex(Ptr);
		const int32 B = GeometryIndexBuffer.GetIndex(Ptr + 1);
		const int32 C = GeometryIndexBuffer.GetIndex(Ptr + 2);

		const auto* TranslatedA = Algo::FindByPredicate(UsedIndices, [A](const auto& Tuple)
		{
			return Tuple.template Get<1>().Contains(A);
		});
		const auto* TranslatedB = Algo::FindByPredicate(UsedIndices, [B](const auto& Tuple)
		{
			return Tuple.template Get<1>().Contains(B);
		});
		const auto* TranslatedC = Algo::FindByPredicate(UsedIndices, [C](const auto& Tuple)
		{
			return Tuple.template Get<1>().Contains(C);
		});

		// A - B
		if (TranslatedA && TranslatedB)
		{
			Adjacency.FindOrAdd(TranslatedA->Key).AddUnique(TranslatedB->Key);
			Adjacency.FindOrAdd(TranslatedB->Key).AddUnique(TranslatedA->Key);
		}
		// B - C
		if (TranslatedB && TranslatedC)
		{
			Adjacency.FindOrAdd(TranslatedB->Key).AddUnique(TranslatedC->Key);
			Adjacency.FindOrAdd(TranslatedC->Key).AddUnique(TranslatedB->Key);
		}
		// C - A
		if (TranslatedC && TranslatedA)
		{
			Adjacency.FindOrAdd(TranslatedC->Key).AddUnique(TranslatedA->Key);
			Adjacency.FindOrAdd(TranslatedA->Key).AddUnique(TranslatedC->Key);
		}
	}

	// Calculate centroid for angle calculation
	FVector Centroid = FVector::ZeroVector;
	for (const auto& Pos : Positions)
		Centroid += Pos;
	Centroid /= Positions.Num();

	// Re-arrange them in a clockwise pattern
	TArray<int32> Loop;
	Loop.Init(0, Adjacency.Num());
	{
		int32 Prev = INDEX_NONE;
		int32 Current = 0;
		int32 Ptr = 0;

		do
		{
			const TArray<int32>& Neighbors = Adjacency[Current];
			// Degree should be 2 for a manifold boundary loop; if not, your input data is broken.
			int32 Next = Neighbors[0] == Prev ? Neighbors[1] : Neighbors[0];
			Prev = Current;
			Current = Next;
			if (Current != 0)
				Loop[Ptr++] = Current;
		}
		while (Current != 0);

		// Flip if it's not clockwise
		float SignedArea = 0.f;
		for (int32 i = 0; i < Loop.Num(); ++i)
		{
			const FVector& P0 = Positions[Loop[i]];
			const FVector& P1 = Positions[Loop[(i + 1) % Loop.Num()]];
			SignedArea += (P0.X * P1.Z - P1.X * P0.Z);
		}
		if (SignedArea < 0.f)
			Algo::Reverse(Loop);
	}

	return Loop;
}


void USplineToolkitMeshExtruder::ComputeMesh(const FSplineToolkitExtrusionRule& Rule,
                                             UProceduralMeshComponent* MeshComponent,
                                             const FSplineToolkitExtruderMeshData& Data) const
{
	FSplineToolkitExtruderDrawData DrawData{};
	DrawData.Init((Rule.NumRmfSamples + 2) * Data.OriginSlice.Num());

	const float TotalSplineDistance = this->SplineComponent->GetSplineLength();

	uint32 VertexPtr = 0;
	for (int32 I = -1; I < Rule.NumRmfSamples + 1; ++I)
	{
		const auto& Sample = Data.RmfSamples[FMath::Clamp(I, 0, Rule.NumRmfSamples - 1)];

		// Instantiate a slice per sample
		DrawData.Insert(Data.OriginSlice, VertexPtr);

		// Create a transform matrix
		FMatrix Rotation{
			Sample.Bitangent.GetSafeNormal(), Sample.Tangent.GetSafeNormal(), Sample.Reference.GetSafeNormal(),
			FVector::ZeroVector
		};
		FTransform Transform;
		Transform.SetComponents(Rotation.ToQuat(), Sample.Position, FVector::OneVector);

		// Transform all vertices with this matrix
		for (int32 Vertex = 0; Vertex < Data.OriginSlice.Num(); ++Vertex, ++VertexPtr)
		{
			DrawData.Positions[VertexPtr] = Transform.TransformPosition(
				(DrawData.Positions[VertexPtr] * Rule.Scale) + Rule.Offset);
			if (I == -1)
				DrawData.Normals[VertexPtr] = -Sample.Tangent;
			else if (I == Rule.NumRmfSamples)
				DrawData.Normals[VertexPtr] = Sample.Tangent;
			else
				DrawData.Normals[VertexPtr] = Transform.TransformVector(DrawData.Normals[VertexPtr]);
			DrawData.Tangents[VertexPtr].TangentX = Transform.TransformVector(DrawData.Tangents[VertexPtr].TangentX);
			// Set UVs to distance / totalDistance
			DrawData.Uv0[VertexPtr].Y = Sample.Distance / TotalSplineDistance;
		}
	}

	/*
	 * Create triangles
	 */
	const int32 SliceCount = Data.OriginSlice.Num();
	const int32 EdgesPerRing = SliceCount;

	// Start and end cap
	const uint32 NumConnectionsIndices = (Rule.NumRmfSamples + 1) * Data.OriginSlice.Num() * 6;

	const auto StartCap = ComputeEndCap(Data, 0, true);
	const auto EndCap = ComputeEndCap(Data, (Rule.NumRmfSamples + 1) * Data.OriginSlice.Num(), false);

	TArray<int32> Indices{};

	const uint32 NumCapsIndices = StartCap.Num() + EndCap.Num();
	Indices.Init(0, NumConnectionsIndices + NumCapsIndices);

	// Insert end caps
	Indices.Insert(StartCap, 0);
	Indices.Insert(EndCap, StartCap.Num() + NumConnectionsIndices);

	// Linking samples
	uint32 Ptr = StartCap.Num();
	for (uint32 SampleIdx = 0; SampleIdx < static_cast<uint32>(Rule.NumRmfSamples) + 1; ++SampleIdx)
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

			Indices[Ptr++] = A;
			Indices[Ptr++] = B;
			Indices[Ptr++] = C;
			Indices[Ptr++] = B;
			Indices[Ptr++] = D;
			Indices[Ptr++] = C;
		}
	}

	MeshComponent->CreateMeshSection(0, DrawData.Positions, Indices, DrawData.Normals, DrawData.Uv0, {},
	                                 DrawData.Tangents, true);
}


void USplineToolkitMeshExtruder::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (this->Ruleset->IsValidLowLevel())
		this->Ruleset->OnChanged.AddLambda([this]
		{
			Regenerate();
		});
}
