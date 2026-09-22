#include "SplineToolkitModifier.h"

#include "SplineToolkitRuleset.h"
#include "Components/SplineComponent.h"

FSplineToolkitStepContext::FSplineToolkitStepContext(USplineComponent* Spline, float Distance)
{
	SplineComponent = Spline;
	TimeAlongSpline = Spline->GetTimeAtDistanceAlongSpline(Distance);
	DistanceAlongSpline = Distance;
	bCross = false; // TODO: Implement this pls Patrick :D
	bSplit = false; // TODO: Implement once we got the components finished up
	Curvature = GetCurvatureAtDistanceAlongSpline(SplineComponent, Distance);
	WorldTransform = Spline->GetTransformAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
}
