// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer


#include "SplineToolkitConnector.h"

#include "EngineUtils.h"

void USplineToolkitConnector::OnRegister()
{
	Super::OnRegister();

	if (AActor* Owner = GetOwner())
	{
		if (!Owner->FindComponentByClass<USplineComponent>())
		{
			UE_LOG(LogTemp, Error, TEXT("Instantiator requires USplineComponent"));
			return;
		}
		SplineComponent = Owner->GetComponentByClass<USplineComponent>();
		SplineComponent->GetOnSplineChanged().AddLambda([this] { if (bAutoUpdate & !bUpdateActive) ReAttach(); });
	}
}

void USplineToolkitConnector::Attach(const FSplineConnection& Connection)
{
	Connections.AddUnique(Connection);
	Validate();
}

void USplineToolkitConnector::AutoAttach(USplineComponent* Target)
{
	FSplineConnection Con = {};
	
	FVector ThisStart = SplineComponent->GetLocationAtTime(0.0f, ESplineCoordinateSpace::World);
	FVector ThisEnd   = SplineComponent->GetLocationAtTime(1.0f, ESplineCoordinateSpace::World);
	
	FVector OtherStart = Target->GetLocationAtTime(0.0f, ESplineCoordinateSpace::World);
	FVector OtherEnd   = Target->GetLocationAtTime(1.0f, ESplineCoordinateSpace::World);
	
	Con.ToSpline = Target;
	
	float DistA = (ThisStart - OtherStart).Length();
	float DistB = (ThisStart - OtherEnd).Length();
	if (DistB < DistA) Con.bToEnd = true;
	float DistC = (ThisEnd - OtherStart).Length();
	if (DistC < DistB) { Con.bFromEnd = true; Con.bToEnd = false; }
	float DistD = (ThisEnd - OtherEnd).Length();
	if (DistD < DistC) Con.bToEnd = true;
	
	Connections.AddUnique(Con);
	ReAttach();
}

void USplineToolkitConnector::FullAutoAttach()
{
	float ClosestDistance = 10000.0f;
	FSplineConnection ClosestCon = {};
	
	auto ThisStart = SplineComponent->GetLocationAtTime(0, ESplineCoordinateSpace::World);
	auto ThisEnd = SplineComponent->GetLocationAtTime(1.0f, ESplineCoordinateSpace::World);
	
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor) continue;
		if (Actor == GetOwner()) continue;
		USplineComponent* SplineComp = Actor->GetComponentByClass<USplineComponent>();
		if (!SplineComp) continue;
		
		bool Existing = false;
		for (const auto& Con : Connections)
		{
			if (Con.ToSpline != SplineComp) continue; 
			Existing = true;
			break;
		}
		if (Existing) continue;
		
		FSplineConnection Con = {};
		Con.ToSpline = SplineComp;
	
		FVector OtherStart = SplineComp->GetLocationAtTime(0.0f, ESplineCoordinateSpace::World);
		FVector OtherEnd   = SplineComp->GetLocationAtTime(1.0f, ESplineCoordinateSpace::World);
	
		float DistA = (ThisStart - OtherStart).Length();
		float DistB = (ThisStart - OtherEnd).Length();
		if (DistB < DistA) Con.bToEnd = true;
		float DistC = (ThisEnd - OtherStart).Length();
		if (DistC < DistB) { Con.bFromEnd = true; Con.bToEnd = false; }
		float DistD = (ThisEnd - OtherEnd).Length();
		if (DistD < DistC) Con.bToEnd = true;
		
		float Dist = fminf(DistA, fminf(DistB, fminf(DistC, DistD)));
		
		if (Dist < ClosestDistance)
		{
			ClosestCon = Con;
			ClosestDistance = Dist;
		}
	}
	
	if (ClosestDistance >= 10000.0f) return;
	
	Connections.AddUnique(ClosestCon);
	ReAttach({}, ClosestCon.bFromEnd ? ESplineEnd::End : ESplineEnd::Start);
}

void USplineToolkitConnector::Validate()
{
	// Validate the spline component on this object
	if (!IsValid(SplineComponent))
	{
		if (AActor* Owner = GetOwner())
		{
			if (!Owner->FindComponentByClass<USplineComponent>())
			{
				UE_LOG(LogTemp, Error, TEXT("Instantiator requires USplineComponent"));
				return;
			}
			SplineComponent = Owner->GetComponentByClass<USplineComponent>();
			SplineComponent->GetOnSplineChanged().AddLambda([this] { if (bAutoUpdate) ReAttach(); });
		}
	}
	
	if (Connections.IsEmpty()) return;
	
	// Check that the spline connection is still valid
	for (int i = Connections.Num() - 1; i >= 0; --i)
	{
		if (!IsValid(Connections[i].ToSpline))
			Connections.RemoveAt(i);
		if (Connections[i].ToSpline == SplineComponent)
			Connections.RemoveAt(i);
	}
	
	// Check that the other spline has an opposing connection to this one
	for (const auto& Con : Connections)
	{
		USplineToolkitConnector* Other = Con.ToSpline->GetOwner()->GetComponentByClass<USplineToolkitConnector>();
		
		bool found = false;
		for (const auto& OtherCon : Other->Connections)
			if (OtherCon.IsOpposingConnection(SplineComponent, OtherCon))
			{
				found = true;
				break;
			}
		
		if (found) continue;
		Other->Connections.AddUnique(Con.GetOpposingConnection(SplineComponent));
	}
		
	// Check if at most one connection is enabled
	if (!bAllowMultipleEnabled)
	{
		bool bStartEnabled = false;
		bool bEndEnabled = false;

		for (auto& Con : Connections)
		{
			if (!Con.bEnabled) continue;
				
			if (Con.bFromEnd)
			{
				if (bEndEnabled) Con.bEnabled = false;
				bEndEnabled = true;
			}
			else
			{
				if (bStartEnabled) Con.bEnabled = false;
				bStartEnabled = true;
			}
		}
	}
}

void USplineToolkitConnector::ReAttach(TArray<USplineComponent*> Seen, const ESplineEnd End)
{
	if (End == ESplineEnd::None) return;
	
	Validate();
	
	FVector StartLoc = SplineComponent->GetLocationAtTime(0, ESplineCoordinateSpace::World);
	float StartRoll = SplineComponent->GetRollAtTime(0, ESplineCoordinateSpace::World);
	FVector StartTan = SplineComponent->GetTangentAtTime(0, ESplineCoordinateSpace::World);
	
	FVector EndLoc = SplineComponent->GetLocationAtTime(1.0f, ESplineCoordinateSpace::World);
	float EndRoll = SplineComponent->GetRollAtTime(1.0f, ESplineCoordinateSpace::World);
	FVector EndTan = SplineComponent->GetTangentAtTime(1.0f, ESplineCoordinateSpace::World);
	
	for (auto& Con : Connections)
	{
		if (Seen.Contains(Con.ToSpline)) continue;
		Seen.AddUnique(SplineComponent);
		if (End != ESplineEnd::Both)
		{
			if (Con.bFromEnd && End != ESplineEnd::End) continue;
			if (!Con.bFromEnd && End != ESplineEnd::Start) continue;
		}
		
		const auto PointIdx = Con.bToEnd ? Con.ToSpline->GetNumberOfSplinePoints() - 1 : 0;
		const auto Loc = Con.bFromEnd ? EndLoc : StartLoc;
		const auto Roll = Con.bFromEnd ? EndRoll : StartRoll;
		auto Tan = Con.bFromEnd ? EndTan : StartTan;
		if (Con.IsInvertedConnection()) 
			Tan = -Tan;
		auto Rot = FRotationMatrix::MakeFromX(Tan).Rotator();
		Rot.Roll = Con.IsInvertedConnection() ? -Roll : Roll;
		
		auto ToConnector = Con.ToSpline->GetOwner()->GetComponentByClass<USplineToolkitConnector>();
		
		ToConnector->bUpdateActive = true;
		Con.ToSpline->SetLocationAtSplinePoint(PointIdx, Loc, ESplineCoordinateSpace::World);
		Con.ToSpline->SetRotationAtSplinePoint(PointIdx, Rot, ESplineCoordinateSpace::World);
		Con.ToSpline->SetTangentAtSplinePoint(PointIdx, Tan, ESplineCoordinateSpace::World);
		ToConnector->bUpdateActive = false;
		ToConnector->ReAttach(Seen, Con.bToEnd ? ESplineEnd::End : ESplineEnd::Start);
	}
}

