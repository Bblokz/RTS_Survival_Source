#include "FormationDragPath.h"

namespace FormationDragPathPrivate
{
	// Cursor jitter below this distance does not become part of the path.
	constexpr float InitialMinimumPointSpacing = 40.0f;
	constexpr int32 MaximumPointCount = 128;
	// The facing at a point is taken from the path this far to either side, which smooths out a shaky drag.
	constexpr float FacingSmoothingDistance = 120.0f;

	FVector GetFacingForLineDirection(const FVector& LineDirection)
	{
		// The drag direction is the formation's right, so its forward is that direction turned a quarter left.
		return FVector(LineDirection.Y, -LineDirection.X, 0.0f);
	}
}

void FFormationDragPath::Start(const FVector& StartLocation)
{
	Reset();
	M_Points.Add(StartLocation);
	M_CumulativeLengths.Add(0.0f);
}

void FFormationDragPath::Reset()
{
	M_Points.Reset();
	M_CumulativeLengths.Reset();
	M_MinimumPointSpacing = FormationDragPathPrivate::InitialMinimumPointSpacing;
}

bool FFormationDragPath::TryAddPoint(const FVector& Location)
{
	if (not GetIsStarted())
	{
		return false;
	}
	const float DistanceFromLastPoint = FVector::Dist2D(M_Points.Last(), Location);
	if (DistanceFromLastPoint < M_MinimumPointSpacing)
	{
		return false;
	}
	M_CumulativeLengths.Add(GetLength() + DistanceFromLastPoint);
	M_Points.Add(Location);
	if (M_Points.Num() >= FormationDragPathPrivate::MaximumPointCount)
	{
		RemoveEveryOtherPoint();
	}
	return true;
}

FVector FFormationDragPath::GetLocationAtDistance(const float Distance) const
{
	if (M_Points.IsEmpty())
	{
		return FVector::ZeroVector;
	}
	if (Distance <= 0.0f || M_Points.Num() == 1)
	{
		return M_Points[0];
	}
	for (int32 PointIndex = 1; PointIndex < M_Points.Num(); ++PointIndex)
	{
		if (Distance > M_CumulativeLengths[PointIndex])
		{
			continue;
		}
		const float SegmentStartLength = M_CumulativeLengths[PointIndex - 1];
		const float SegmentLength = M_CumulativeLengths[PointIndex] - SegmentStartLength;
		const float SegmentAlpha = SegmentLength > KINDA_SMALL_NUMBER
			                           ? (Distance - SegmentStartLength) / SegmentLength
			                           : 0.0f;
		return FMath::Lerp(M_Points[PointIndex - 1], M_Points[PointIndex], SegmentAlpha);
	}
	return M_Points.Last();
}

FVector FFormationDragPath::GetFacingAtDistance(const float Distance) const
{
	using namespace FormationDragPathPrivate;
	const FVector LocationBehind = GetLocationAtDistance(Distance - FacingSmoothingDistance);
	const FVector LocationAhead = GetLocationAtDistance(Distance + FacingSmoothingDistance);
	const FVector LocalDirection = (LocationAhead - LocationBehind).GetSafeNormal2D();
	if (LocalDirection.IsNearlyZero())
	{
		return GetOverallFacing();
	}
	return GetFacingForLineDirection(LocalDirection);
}

FVector FFormationDragPath::GetOverallFacing() const
{
	if (M_Points.Num() < 2)
	{
		return FVector::ForwardVector;
	}
	const FVector OverallDirection = (M_Points.Last() - M_Points[0]).GetSafeNormal2D();
	if (OverallDirection.IsNearlyZero())
	{
		// The drag ended where it began; any facing is as good as another.
		return FVector::ForwardVector;
	}
	return FormationDragPathPrivate::GetFacingForLineDirection(OverallDirection);
}

void FFormationDragPath::RemoveEveryOtherPoint()
{
	// The first and last point stay, so the path keeps its ends.
	const int32 LastRemovableIndex = M_Points.Num() - 2;
	const int32 FirstOddIndexFromEnd = LastRemovableIndex % 2 == 0 ? LastRemovableIndex - 1 : LastRemovableIndex;
	for (int32 PointIndex = FirstOddIndexFromEnd; PointIndex >= 1; PointIndex -= 2)
	{
		M_Points.RemoveAt(PointIndex);
	}
	M_MinimumPointSpacing *= 2.0f;
	RebuildCumulativeLengths();
}

void FFormationDragPath::RebuildCumulativeLengths()
{
	M_CumulativeLengths.Reset(M_Points.Num());
	float LengthSoFar = 0.0f;
	for (int32 PointIndex = 0; PointIndex < M_Points.Num(); ++PointIndex)
	{
		if (PointIndex > 0)
		{
			LengthSoFar += FVector::Dist2D(M_Points[PointIndex - 1], M_Points[PointIndex]);
		}
		M_CumulativeLengths.Add(LengthSoFar);
	}
}
