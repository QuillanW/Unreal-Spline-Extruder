#include "SplineToolkitModifier.h"

#include "SplineToolkitRuleset.h"
#include "Components/SplineComponent.h"

FSplineToolkitStepContext::FSplineToolkitStepContext(USplineComponent* Spline, FSplineToolkitRmfSample Sample)
{
	SplineComponent = Spline;
	RMFSample = Sample;
	bCross = false; // TODO: Implement this pls Patrick :D
	bSplit = false; // TODO: Implement once we got the components finished up
	WorldTransform = Spline->GetTransformAtDistanceAlongSpline(Sample.Distance, ESplineCoordinateSpace::World);
}
