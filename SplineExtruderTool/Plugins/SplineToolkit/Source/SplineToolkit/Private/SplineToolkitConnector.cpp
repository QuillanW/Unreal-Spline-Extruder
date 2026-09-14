// Copyright 2026 Patrick Vreeburg + Quillan Wielhouwer


#include "SplineToolkitConnector.h"

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
		SplineComponent->GetOnSplineChanged().AddLambda([this] { if (bAutoUpdate) ReAttach(); });
	}
}

void USplineToolkitConnector::Attach(const FSplineConnection& Connection)
{
	Connections.Add(Connection);
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
	
	Connections.Add(Con);
	Validate();
}

void USplineToolkitConnector::Validate()
{
	// Check that the spline connection is still valid
	for (int i = Connections.Num() - 1; i < 0; --i)
	{
		if (!IsValid(Connections[i].ToSpline))
			Connections.RemoveAt(i);
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

void USplineToolkitConnector::ReAttach()
{
	Validate();
	
	FVector StartLoc = SplineComponent->GetLocationAtTime(0, ESplineCoordinateSpace::World);
	FVector StartTan = SplineComponent->GetTangentAtTime(0, ESplineCoordinateSpace::World);
	float StartRoll = SplineComponent->GetRollAtTime(0, ESplineCoordinateSpace::World);
	
	FVector EndLoc = SplineComponent->GetLocationAtTime(1.0f, ESplineCoordinateSpace::World);
	
	for (auto& Con : Connections)
	{
		const auto PointIdx = Con.bToEnd ? Con.ToSpline->GetNumberOfSplinePoints() : 0;
		Con.ToSpline->SetWorldLocationAtSplinePoint(PointIdx, );
		Con.ToSpline->SetTangentAtSplinePoint(PointIdx, );
	}
}

