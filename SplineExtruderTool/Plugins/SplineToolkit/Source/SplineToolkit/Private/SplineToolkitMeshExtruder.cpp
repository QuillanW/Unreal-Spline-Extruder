// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer
#include "SplineToolkitMeshExtruder.h"

#include "SplineToolkitModifier.h"
#include "Components/SplineComponent.h"
#include "Misc/Zip.h"

int32 GSplineToolkitShowExtruderIndices = 0;
static FAutoConsoleVariableRef CVarShowExtruderIndices(
	TEXT("stk.Extruder.ShowIndices"),
	GSplineToolkitShowExtruderIndices,
	TEXT(
		"Shows the indices of the start cap. This is useful when debugging the code in the plugin itself. Only works in PIE/Runtime."));


#pragma region DrawData impl

void FSplineToolkitExtruderDrawData::ReserveVertices(uint32 NumVertices)
{
	this->Positions.Reserve(NumVertices);
	this->Normals.Reserve(NumVertices);
	this->Tangents.Reserve(NumVertices);
	this->Uv0.Reserve(NumVertices);
}


void FSplineToolkitExtruderDrawData::InitVertices(uint32 NumVertices)
{
	this->Positions.Init(FVector::ZeroVector, NumVertices);
	this->Normals.Init(FVector::ZeroVector, NumVertices);
	this->Tangents.Init(FProcMeshTangent{FVector::ZeroVector, false}, NumVertices);
	this->Uv0.Init(FVector2D::ZeroVector, NumVertices);
}


void FSplineToolkitExtruderDrawData::ReserveIndices(uint32 NumIndices) { this->Indices.Reserve(NumIndices); }
void FSplineToolkitExtruderDrawData::InitIndices(uint32 NumIndices) { this->Indices.Init(0, NumIndices); }


void FSplineToolkitExtruderDrawData::InsertVertices(const FSplineToolkitExtruderDrawData& Other, uint32 Where)
{
	this->Positions.Insert(Other.Positions.GetData(), Other.Positions.Num(), Where);
	this->Normals.Insert(Other.Normals.GetData(), Other.Normals.Num(), Where);
	this->Tangents.Insert(Other.Tangents.GetData(), Other.Tangents.Num(), Where);
	this->Uv0.Insert(Other.Uv0.GetData(), Other.Uv0.Num(), Where);
	VertexTopIdx = Where + Other.VertexNum();
}


void FSplineToolkitExtruderDrawData::AppendIndices(TArray<int32>&& List)
{
	const auto Index = this->IndexTopIdx;
	this->IndexTopIdx += List.Num();
	this->Indices.Insert(MoveTemp(List), Index);
}


void FSplineToolkitExtruderDrawData::AddIndex(int32 Index) { this->Indices[IndexTopIdx++] = Index; }


void FSplineToolkitExtruderDrawData::ShrinkFit()
{
	this->Positions.SetNum(VertexTopIdx);
	this->Normals.SetNum(VertexTopIdx);
	this->Tangents.SetNum(VertexTopIdx);
	this->Uv0.SetNum(VertexTopIdx);
	this->Indices.SetNum(IndexTopIdx);
}


int32 FSplineToolkitExtruderDrawData::VertexTop() const { return this->VertexTopIdx; }
int32 FSplineToolkitExtruderDrawData::IndexTop() const { return this->IndexTopIdx; }

int32 FSplineToolkitExtruderDrawData::VertexNum() const { return this->Positions.Num(); }
bool FSplineToolkitExtruderDrawData::IsEmpty() const { return this->Positions.IsEmpty(); }

#pragma endregion


void USplineToolkitMeshExtruder::OnRegister()
{
	Super::OnRegister();

	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;

	if (const AActor* Owner = GetOwner())
	{
		if (!Owner->FindComponentByClass<USplineComponent>())
		{
			UE_LOG(LogTemp, Error, TEXT("Extruder requires USplineComponent"));
			return;
		}
		this->SplineComponent = Owner->GetComponentByClass<USplineComponent>();
		this->SplineComponent->GetOnSplineChanged().AddLambda([this]()
		{
			if (this->bUpdateOnSplineChange)
				Regenerate();
		});
	}

	if (this->Ruleset)
	{
		this->Ruleset->OnShouldRegenerate.AddLambda([this]
		{
			if (this->bUpdateOnRulesetChange)
				Regenerate();
		});
		this->Ruleset->OnReapplyMaterials.AddLambda([this]
		{
			if (this->bUpdateOnRulesetChange)
				ReapplyMaterials();
		});
	}

	for (auto& Data : this->OutMeshes)
	{
		if (IsValid(Data.MeshActor))
		{
			Data.MeshActor->AttachToActor(GetOwner(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			UE_LOG(LogTemp, Warning, TEXT("Owner root: %s, MeshActor root: %s"),
			       *GetNameSafe(GetOwner()->GetRootComponent()),
			       *GetNameSafe(Data.MeshActor->GetRootComponent()));

			Data.ProceduralMeshComponent = Data.MeshActor->GetComponentByClass<UProceduralMeshComponent>();
		}
	}
}


void USplineToolkitMeshExtruder::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                               FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GSplineToolkitShowExtruderIndices)
	{
		auto* RmfSampler = GetOwner()->FindComponentByClass<USplineToolkitRmfSampler>();
		for (const auto& Data : this->OutMeshes)
		{
			if (!RmfSampler)
				break;

			// Create a transform matrix
			const auto& Sample = RmfSampler->Samples[0];
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


void USplineToolkitMeshExtruder::MarkDirty()
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
		ExtractOriginSlice(Rule.Mesh, Data);

		if (auto* MeshComponent = Data.MeshActor->FindComponentByClass<UProceduralMeshComponent>();
			!Data.OriginSlice.IsEmpty() && MeshComponent)
		{
			ComputeMesh(Rule, MeshComponent, Data);
		}
	}
}


void USplineToolkitMeshExtruder::ReapplyMaterials()
{
	for (const auto& [Rule, Data] : UE::Zip(this->Ruleset->ExtrusionRules, this->OutMeshes))
	{
		if (!IsValid(Data.ProceduralMeshComponent))
			continue;
		Data.ProceduralMeshComponent->SetMaterial(0, Rule.Material);
		Data.ProceduralMeshComponent->SetOverlayMaterial(Rule.OverlayMaterial);
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
	Data.OriginSlice.ReserveVertices(MeshPositions.GetNumVertices());

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
		for (int32 I = 0; I < Data.OriginSlice.VertexNum(); ++I)
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
	SortedData.InitVertices(SortedIndices.Num());

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


USplineToolkitRulesetModifierBase* USplineToolkitMeshExtruder::GetOrCreateModifierInstance(
	TSubclassOf<USplineToolkitRulesetModifierBase> Class)
{
	if (USplineToolkitRulesetModifierBase** Found = ModifierInstanceCache.Find(Class))
	{
		return *Found;
	}
	USplineToolkitRulesetModifierBase* NewInstance = NewObject<USplineToolkitRulesetModifierBase>(
		GetTransientPackage(), Class);
	ModifierInstanceCache.Add(Class, NewInstance);
	return NewInstance;
}


// Generated by Claude Sonnet 5 with some tweaks to make it work for my application
TArray<int32> USplineToolkitMeshExtruder::ComputeEndCap(const FSplineToolkitExtruderMeshData& Data, int32 IndexOffset,
                                                        bool InvertOrdering)
{
	TArray<int32> Indices{};

	TArray<TPair<int32, FVector2D>> Points{};
	Points.Reserve(Data.OriginSlice.VertexNum());
	for (int32 I = 0; I < Data.OriginSlice.VertexNum(); ++I)
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


void USplineToolkitMeshExtruder::AddStartCap(USplineToolkitIntersectionSolver* Solver,
                                             FSplineToolkitExtruderDrawData& DrawData,
                                             const FSplineToolkitExtrusionRule& Rule,
                                             const FSplineToolkitExtruderMeshData& Data) const
{
	FSplineToolkitRmfSample Sample{
		.Distance = -1.f
	};
	int32 VertexPtr = 0;
	bool _;
	AddNextSampleToMesh(Solver, DrawData, Rule, Data, Sample, VertexPtr, _);

	const int32 IterEnd = VertexPtr - Data.OriginSlice.VertexNum();
	while (VertexPtr >= IterEnd)
	{
		DrawData.Positions[VertexPtr] -= KINDA_SMALL_NUMBER * Sample.Tangent;
		DrawData.Normals[VertexPtr--] = -Sample.Tangent;
	}
}


void USplineToolkitMeshExtruder::AddEndCap(USplineToolkitIntersectionSolver* Solver,
                                           FSplineToolkitExtruderDrawData& DrawData,
                                           const FSplineToolkitExtrusionRule& Rule,
                                           const FSplineToolkitExtruderMeshData& Data) const
{
	FSplineToolkitRmfSample Sample{
		.Distance = FLT_MAX
	};
	int32 VertexPtr = 0;
	bool _;
	AddNextSampleToMesh(Solver, DrawData, Rule, Data, Sample, VertexPtr, _);

	const int32 IterEnd = VertexPtr - Data.OriginSlice.VertexNum();
	while (VertexPtr >= IterEnd)
	{
		DrawData.Positions[VertexPtr] += KINDA_SMALL_NUMBER * Sample.Tangent;
		DrawData.Normals[VertexPtr--] = Sample.Tangent;
	}
}


bool USplineToolkitMeshExtruder::AddNextSampleToMesh(USplineToolkitIntersectionSolver* Solver,
                                                     FSplineToolkitExtruderDrawData& DrawData,
                                                     const FSplineToolkitExtrusionRule& Rule,
                                                     const FSplineToolkitExtruderMeshData& Data,
                                                     FSplineToolkitRmfSample& OutRmfSample,
                                                     int32& OutVertexPtr,
                                                     bool& OutDontConnect,
                                                     bool bCalledFromSelf) const
{
	auto* RmfSampler = GetOwner()->FindComponentByClass<USplineToolkitRmfSampler>();
	if (!RmfSampler)
		return false;

	OutDontConnect = false;
	if (FMath::Abs(OutRmfSample.Distance - SplineComponent->GetSplineLength()) < KINDA_SMALL_NUMBER)
		return false;

	auto OldSample = OutRmfSample;
	if (OutRmfSample.Distance == -1.f)
	{
		// This should sample the first one
		OutRmfSample = RmfSampler->Samples[0];
	}
	else if (OutRmfSample.Distance == FLT_MAX)
	{
		// This should sample the last one
		OutRmfSample = RmfSampler->Samples.Last();
	}
	else if (!bCalledFromSelf)
	{
		// find the next sample based on distance
		OutRmfSample = RmfSampler->GetNextSampleFromDistance(OutRmfSample.Distance);
	}

	if (IsValid(Solver) && !bCalledFromSelf)
	{
		// Determine the next RMF sample
		auto FindCollisionEnd = [&](
			TCheckedPointerIterator<TArray<FSplineToolkitSplineIntersection>::ElementType, TArray<
				                        FSplineToolkitSplineIntersection>::SizeType> Iter, float RunningMax,
			auto& Self) -> TPair<decltype(Iter), float>
		{
			RunningMax = FMath::Max(RunningMax, Iter->DistanceMax);
			auto Next = Iter + 1;

			if (Next == Solver->Collisions.end())
				return {Iter, RunningMax};

			if (Next->DistanceMin <= RunningMax)
				return Self(Next, RunningMax, Self);

			return {Iter, RunningMax};
		};

		for (auto Iter = Solver->Collisions.begin(); Iter != Solver->Collisions.end(); ++Iter)
		{
			const auto& Cut = *Iter;
			if (FMath::IsWithin(OutRmfSample.Distance, Cut.DistanceMin, Cut.DistanceMax))
			{
				// First add the beginning of the range
				auto BeginSample = RmfSampler->GetSampleAtDistance(Cut.DistanceMin);
				AddNextSampleToMesh(Solver, DrawData, Rule, Data, BeginSample, OutVertexPtr, OutDontConnect, true);
				ConnectToPreviousSample(DrawData, OutVertexPtr, Data);

				const TPair<decltype(Iter), float> CutEnd = FindCollisionEnd(Iter, Iter->DistanceMax, FindCollisionEnd);
				Iter = CutEnd.Get<0>();

				OutRmfSample = RmfSampler->GetSampleAtDistance(CutEnd.Get<1>());
				OutDontConnect = true;
			}
		}
	}

	if (FMath::Abs(OutRmfSample.Distance - OldSample.Distance) < KINDA_SMALL_NUMBER)
	{
		OutDontConnect = true;
		return OutRmfSample != RmfSampler->Samples.Last();
	}

	const float TotalSplineDistance = RmfSampler->Samples.Last().Distance;

	FSplineToolkitStepContext Context{SplineComponent, OutRmfSample};
	FSplineToolkitExtrusionRule ModdedRule = Rule;
	for (USplineToolkitRulesetModifierBase* Modifier : Rule.Modifiers)
	{
		if (!Modifier) continue;
		ModdedRule = Modifier->ModifyExtrusionStep(Context, ModdedRule);
	}

	// Instantiate a slice per sample
	OutVertexPtr = DrawData.VertexTop();
	DrawData.InsertVertices(Data.OriginSlice, OutVertexPtr);

	// Create a transform matrix
	FMatrix Rotation{
		OutRmfSample.Bitangent.GetSafeNormal(),
		OutRmfSample.Tangent.GetSafeNormal(),
		OutRmfSample.Reference.GetSafeNormal(),
		FVector::ZeroVector
	};
	FTransform Transform;

	if (Rotation.ContainsNaN())
		return true;

	Transform.SetComponents(Rotation.ToQuat(), OutRmfSample.Position, FVector::OneVector);

	// Transform all vertices with this matrix
	const int32 IterEnd = OutVertexPtr + Data.OriginSlice.VertexNum();
	for (; OutVertexPtr < IterEnd; ++OutVertexPtr)
	{
		DrawData.Positions[OutVertexPtr] = Transform.TransformPosition(
			(DrawData.Positions[OutVertexPtr] * FVector{ModdedRule.Scale.X, 1.f, ModdedRule.Scale.Y}) + ModdedRule.
			Offset);
		DrawData.Normals[OutVertexPtr] = Transform.TransformVector(DrawData.Normals[OutVertexPtr]);
		DrawData.Tangents[OutVertexPtr].TangentX = Transform.TransformVector(DrawData.Tangents[OutVertexPtr].TangentX);
		// Set UVs to distance / totalDistance
		DrawData.Uv0[OutVertexPtr].Y = (OutRmfSample.Distance / TotalSplineDistance) * ModdedRule.UvScale;
	}

	return true;
}


void USplineToolkitMeshExtruder::ConnectToPreviousSample(FSplineToolkitExtruderDrawData& DrawData, int32 StartIndex,
                                                         const FSplineToolkitExtruderMeshData& Data) const
{
	/*
	 * Create triangles
	 */
	const int32 SliceCount = Data.OriginSlice.VertexNum();
	const int32 EdgesPerRing = SliceCount;

	// Linking samples
	const int32 CurrentRing = StartIndex - 2 * SliceCount;
	const int32 NextRing = CurrentRing + SliceCount;

	// Don't connect when there is no previous ring
	if (CurrentRing < 0)
		return;

	for (int32 V = 0; V < EdgesPerRing; ++V)
	{
		const int32 NextV = (V + 1) % SliceCount;

		const int32 A = CurrentRing + V;
		const int32 B = CurrentRing + NextV;
		const int32 C = NextRing + V;
		const int32 D = NextRing + NextV;

		DrawData.AddIndex(A);
		DrawData.AddIndex(B);
		DrawData.AddIndex(C);
		DrawData.AddIndex(B);
		DrawData.AddIndex(D);
		DrawData.AddIndex(C);
	}
}


void USplineToolkitMeshExtruder::ComputeMesh(const FSplineToolkitExtrusionRule& Rule,
                                             UProceduralMeshComponent* MeshComponent,
                                             const FSplineToolkitExtruderMeshData& Data) const
{
	// A nullptr solver means it just does not take it into account
	auto* Solver = (this->bIgnoreIntersectCutouts)
		               ? nullptr
		               : GetOwner()->FindComponentByClass<USplineToolkitIntersectionSolver>();

	auto* RmfSampler = GetOwner()->FindComponentByClass<USplineToolkitRmfSampler>();

	if (!RmfSampler)
		return;

	FSplineToolkitExtruderDrawData DrawData{};
	DrawData.InitVertices((RmfSampler->NumRmfSamples + 2) * Data.OriginSlice.VertexNum());

	// Start and end cap
	const uint32 MaxNumConnectionsIndices = (RmfSampler->NumRmfSamples + 1) * Data.OriginSlice.VertexNum() * 6;

	bool bNoStartCap = Solver && !Solver->Collisions.IsEmpty() && Solver->Collisions[0].DistanceMin == 0.f;
	if (bNoStartCap)
	{
		// Skip start cap on 0
		DrawData.InitIndices(MaxNumConnectionsIndices);
	}
	else
	{
		AddStartCap(Solver, DrawData, Rule, Data);
		auto StartCap = ComputeEndCap(Data, 0, true);

		const uint32 NumCapsIndices = 2 * StartCap.Num();
		DrawData.InitIndices(MaxNumConnectionsIndices + NumCapsIndices);

		// Insert end caps
		DrawData.AppendIndices(MoveTemp(StartCap));
	}

	int32 VertexPtr = bNoStartCap ? 0 : Data.OriginSlice.VertexNum();
	FSplineToolkitRmfSample Sample{
		.Distance = -1.f
	};
	bool bDontConnect = false;
	while (AddNextSampleToMesh(Solver, DrawData, Rule, Data, Sample, VertexPtr, bDontConnect))
	{
		if (!bDontConnect)
			ConnectToPreviousSample(DrawData, VertexPtr, Data);
	}

	bool bNoEndCap = Solver && !Solver->Collisions.IsEmpty() && FMath::IsNearlyEqual(
		Solver->Collisions.Last().DistanceMax, SplineComponent->GetSplineLength(), KINDA_SMALL_NUMBER);
	if (!bNoEndCap)
	{
		AddEndCap(Solver, DrawData, Rule, Data);

		auto EndCap = ComputeEndCap(Data, DrawData.VertexTop() - Data.OriginSlice.VertexNum(), false);
		DrawData.AppendIndices(MoveTemp(EndCap));
	}

	DrawData.ShrinkFit();

	MeshComponent->CreateMeshSection(0, DrawData.Positions, DrawData.Indices, DrawData.Normals, DrawData.Uv0, {},
	                                 DrawData.Tangents, true);

	MeshComponent->SetMaterial(0, Rule.Material);
	MeshComponent->SetOverlayMaterial(Rule.OverlayMaterial);
}


void USplineToolkitMeshExtruder::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (this->Ruleset->IsValidLowLevel())
	{
		this->Ruleset->OnShouldRegenerate.AddLambda([this]
		{
			if (this->bUpdateOnRulesetChange)
				Regenerate();
		});
		this->Ruleset->OnReapplyMaterials.AddLambda([this]
		{
			if (this->bUpdateOnRulesetChange)
				ReapplyMaterials();
		});
	}
}
