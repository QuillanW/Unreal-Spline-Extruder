// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/SplineToolkitEditModeInteractiveTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "BaseBehaviors/ClickDragBehavior.h"

// for raycast into World
#include "AssetSelection.h"
#include "CollisionQueryParams.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"

#include "SceneManagement.h"
#include "Components/SplineComponent.h"
#include "Snapping/EditorSnappingManager.h"

// localization namespace
#define LOCTEXT_NAMESPACE "USplineToolkitEditModeInteractiveTool"

/*
 * ToolBuilder
 */

UInteractiveTool* USplineToolkitEditModeInteractiveToolBuilder::BuildTool(const FToolBuilderState & SceneState) const
{
	USplineToolkitEditModeInteractiveTool* NewTool = NewObject<USplineToolkitEditModeInteractiveTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	return NewTool;
}


// JAH TODO: update comments
/*
 * Tool
 */

USplineToolkitEditModeInteractiveToolProperties::USplineToolkitEditModeInteractiveToolProperties()
{
	// initialize the points and distance to reasonable values
	StartPoint = FVector(0,0,0);
	EndPoint = FVector(0,0,100);
	Distance = 100;
}


void USplineToolkitEditModeInteractiveTool::SetWorld(UWorld* World)
{
	check(World);
	this->TargetWorld = World;
}


void USplineToolkitEditModeInteractiveTool::Setup()
{
	UInteractiveTool::Setup();

	// Add default mouse input behavior
	UClickDragInputBehavior* MouseBehavior = NewObject<UClickDragInputBehavior>();
	// We will use the shift key to indicate that we should move the second point. 
	// This call tells the Behavior to call our OnUpdateModifierState() function on mouse-down and mouse-move
	MouseBehavior->Modifiers.RegisterModifier(MoveSecondPointModifierID, FInputDeviceState::IsShiftKeyDown);
	MouseBehavior->Initialize(this);
	AddInputBehavior(MouseBehavior);

	// Create the property set and register it with the Tool
	Properties = NewObject<USplineToolkitEditModeInteractiveToolProperties>(this, "Measurement");
	AddToolPropertySource(Properties);
	
	bSecondPointModifierDown = false;
	bMoveSecondPoint = false;
}


void USplineToolkitEditModeInteractiveTool::OnUpdateModifierState(int ModifierID, bool bIsOn)
{
	// keep track of the "second point" modifier (shift key for mouse input)
	if (ModifierID == MoveSecondPointModifierID)
	{
		bSecondPointModifierDown = bIsOn;
	}
}


FInputRayHit USplineToolkitEditModeInteractiveTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	// we only start drag if press-down is on top of something we can raycast
	FVector Temp;
	FInputRayHit Result = FindRayHit(PressPos.WorldRay, Temp);
	return Result;
}


void USplineToolkitEditModeInteractiveTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	GEditor->SelectNone(false, true, false);
	
	float ClosestDistance = 10000.0f;
	USplineComponent* ClosestComp = nullptr;
	int32 ClosestPointIdx = 0;
	
	for (TActorIterator<AActor> ActorIt(TargetWorld); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor) continue;
		USplineComponent* SplineComp = Actor->GetComponentByClass<USplineComponent>();
		if (!SplineComp) continue;
		
		for (int32 Idx = 0; Idx < SplineComp->GetNumberOfSplinePoints(); ++Idx)
		{
			auto SplinePoint = SplineComp->GetSplinePointAt(Idx, ESplineCoordinateSpace::World);
			FVector ClickedPoint;
			FindRayHit(PressPos.WorldRay, ClickedPoint);
			float Distance = (ClickedPoint - SplinePoint.Position).Length();
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestComp = SplineComp;
				ClosestPointIdx = Idx;
				
				Properties->StartPoint = ClickedPoint;
				Properties->EndPoint = SplinePoint.Position;
			}
		}
	}
	
	
	// for (FSelectionIterator Iter(*SelectedInfo); Iter; ++Iter)
	// {
	// 	
	// 	AActor* Actor = Cast<AActor>(*Iter);
	// 	if (Actor)
	// 	{
	// 		if (Actor->GetComponentByClass<USplineComponent>())
	// 			UE_LOG(LogTemp, Warning, TEXT("SPLINE SELECTED"));
	// 	}
	// }
		
	// determine whether we are moving first or second point for the drag sequence
}


void USplineToolkitEditModeInteractiveTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	UpdatePosition(DragPos.WorldRay);
}


FInputRayHit USplineToolkitEditModeInteractiveTool::FindRayHit(const FRay& WorldRay, FVector& HitPos)
{
	// trace a ray into the World
	FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);
	FHitResult Result;
	bool bHitWorld = TargetWorld->LineTraceSingleByObjectType(Result, WorldRay.Origin, WorldRay.PointAt(999999), QueryParams);
	if (bHitWorld)
	{
		HitPos = Result.ImpactPoint;
		return FInputRayHit(Result.Distance);
	}
	return FInputRayHit();
}


void USplineToolkitEditModeInteractiveTool::UpdatePosition(const FRay& WorldRay)
{
	FInputRayHit HitResult = FindRayHit(WorldRay, (bMoveSecondPoint) ? Properties->EndPoint : Properties->StartPoint);
	if (HitResult.bHit)
	{
		UpdateDistance();
	}
}


void USplineToolkitEditModeInteractiveTool::UpdateDistance()
{
	Properties->Distance = FVector::Distance(Properties->StartPoint, Properties->EndPoint);
}


void USplineToolkitEditModeInteractiveTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	// if the user updated any of the property fields, update the distance
	UpdateDistance();
}


void USplineToolkitEditModeInteractiveTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	// draw a thin line that shows through objects
	PDI->DrawLine(Properties->StartPoint, Properties->EndPoint,
		FColor(240, 16, 16), SDPG_Foreground, 2.0f, 0.0f, true);
	// draw a thicker line that is depth-tested
	PDI->DrawLine(Properties->StartPoint, Properties->EndPoint,
		FColor(240, 16, 16), SDPG_World, 4.0f, 0.0f, true);
}


#undef LOCTEXT_NAMESPACE
