#include "FormationDragLineLayout.h"

#include "FormationDragPath.h"

namespace FormationDragLineLayoutPrivate
{
	// Same row spacing as the rectangle formation, so a short line looks like a rectangle of that width.
	constexpr float RowSpacingDivisor = 1.5f;

	// The front reaches as far to either side of the anchor as the arrow is long.
	constexpr float ArrowLengthToFrontWidth = 2.0f;
	// Slack on the rectangle's front, so rounding cannot push its last unit into the next row.
	constexpr float FrontWidthFitTolerance = 1.0f;
	// A front of a single unit still needs two distinct ends to know which way it faces.
	constexpr float MinimumFrontLineHalfWidth = 1.0f;

	/** Distance between the centres of the first and last unit of a row standing shoulder to shoulder. */
	float ComputeRowSpan(const TArray<float>& RowRadii)
	{
		float RowSpan = 0.0f;
		for (int32 UnitIndex = 1; UnitIndex < RowRadii.Num(); ++UnitIndex)
		{
			RowSpan += FFormationDragLineLayout::GetNeighbourSpacing(RowRadii[UnitIndex - 1], RowRadii[UnitIndex]);
		}
		return RowSpan;
	}

	float ComputeRowMaxRadius(const TArray<float>& RowRadii)
	{
		float RowMaxRadius = 0.0f;
		for (const float UnitRadius : RowRadii)
		{
			RowMaxRadius = FMath::Max(RowMaxRadius, UnitRadius);
		}
		return RowMaxRadius;
	}

	void PlaceRow(
		const TArray<float>& RowRadii,
		const FFormationDragPath& DragPath,
		const float RowDepth,
		const float SpacingScale,
		TArray<FVector>& OutPositions,
		TArray<FRotator>& OutRotations)
	{
		// Centred on the line, so a row that does not reach both ends leaves the same gap at either end.
		float DistanceAlongLine = (DragPath.GetLength() - ComputeRowSpan(RowRadii) * SpacingScale) * 0.5f;
		for (int32 UnitIndex = 0; UnitIndex < RowRadii.Num(); ++UnitIndex)
		{
			if (UnitIndex > 0)
			{
				DistanceAlongLine += SpacingScale *
					FFormationDragLineLayout::GetNeighbourSpacing(RowRadii[UnitIndex - 1], RowRadii[UnitIndex]);
			}
			const FVector Facing = DragPath.GetFacingAtDistance(DistanceAlongLine);
			OutPositions.Add(DragPath.GetLocationAtDistance(DistanceAlongLine) - Facing * RowDepth);
			OutRotations.Add(Facing.Rotation());
		}
	}
}

float FFormationDragLineLayout::GetNeighbourSpacing(const float FirstUnitRadius, const float SecondUnitRadius)
{
	return (FirstUnitRadius + SecondUnitRadius) * 0.5f;
}

float FFormationDragLineLayout::ComputeFrontRowSpan(const TArray<float>& UnitRadii, const int32 FrontRowUnitCount)
{
	const int32 UnitsInFrontRow = FMath::Min(FrontRowUnitCount, UnitRadii.Num());
	float FrontRowSpan = 0.0f;
	for (int32 UnitIndex = 1; UnitIndex < UnitsInFrontRow; ++UnitIndex)
	{
		FrontRowSpan += GetNeighbourSpacing(UnitRadii[UnitIndex - 1], UnitRadii[UnitIndex]);
	}
	return FrontRowSpan;
}

float FFormationDragLineLayout::ComputeArrowFrontWidth(const float RectangleFrontSpan, const float ArrowLength)
{
	using namespace FormationDragLineLayoutPrivate;
	return FMath::Max(RectangleFrontSpan + FrontWidthFitTolerance, ArrowLength * ArrowLengthToFrontWidth);
}

void FFormationDragLineLayout::MakeArrowFrontLine(
	const FFormationArrowDrag& ArrowDrag,
	const float FrontWidth,
	FFormationDragPath& OutFrontLine)
{
	using namespace FormationDragLineLayoutPrivate;
	const FRotator FlatFacing(0.0f, ArrowDrag.Facing.Yaw, 0.0f);
	const FVector FrontDirection = FlatFacing.RotateVector(FVector::RightVector);
	const float HalfWidth = FMath::Max(FrontWidth * 0.5f, MinimumFrontLineHalfWidth);
	// Running from the left flank to the right flank makes the units on the line face along the arrow.
	OutFrontLine.SetStraightLine(
		ArrowDrag.AnchorLocation - FrontDirection * HalfWidth,
		ArrowDrag.AnchorLocation + FrontDirection * HalfWidth);
}

TArray<int32> FFormationDragLineLayout::ComputeRowUnitCounts(const TArray<float>& UnitRadii, const float LineLength)
{
	TArray<int32> RowUnitCounts;
	float CurrentRowSpan = 0.0f;
	for (int32 UnitIndex = 0; UnitIndex < UnitRadii.Num(); ++UnitIndex)
	{
		float SpanWithUnit = 0.0f;
		if (not RowUnitCounts.IsEmpty())
		{
			SpanWithUnit = CurrentRowSpan + GetNeighbourSpacing(UnitRadii[UnitIndex - 1], UnitRadii[UnitIndex]);
		}
		if (RowUnitCounts.IsEmpty() || SpanWithUnit > LineLength)
		{
			RowUnitCounts.Add(0);
			SpanWithUnit = 0.0f;
		}
		CurrentRowSpan = SpanWithUnit;
		++RowUnitCounts.Last();
	}
	return RowUnitCounts;
}

void FFormationDragLineLayout::PlaceRows(
	const TArray<TArray<float>>& RowUnitRadii,
	const FFormationDragPath& DragPath,
	TArray<FVector>& OutPositions,
	TArray<FRotator>& OutRotations)
{
	using namespace FormationDragLineLayoutPrivate;
	OutPositions.Reset();
	OutRotations.Reset();

	const float LineLength = DragPath.GetLength();
	float RowDepth = 0.0f;
	float PreviousRowMaxRadius = 0.0f;
	float PreviousRowSpacingScale = 1.0f;
	for (int32 RowIndex = 0; RowIndex < RowUnitRadii.Num(); ++RowIndex)
	{
		const TArray<float>& RowRadii = RowUnitRadii[RowIndex];
		const float RowMaxRadius = ComputeRowMaxRadius(RowRadii);
		if (RowIndex > 0)
		{
			RowDepth += (PreviousRowMaxRadius + RowMaxRadius) / RowSpacingDivisor;
		}
		PreviousRowMaxRadius = RowMaxRadius;

		const float RowSpan = ComputeRowSpan(RowRadii);
		float SpacingScale = RowSpan > KINDA_SMALL_NUMBER ? FMath::Max(LineLength / RowSpan, 1.0f) : 1.0f;
		// The last row is usually not full; it keeps the spacing of the row before it instead of thinning out.
		const bool bIsTrailingRow = RowIndex > 0 && RowIndex == RowUnitRadii.Num() - 1;
		if (bIsTrailingRow)
		{
			SpacingScale = FMath::Min(SpacingScale, PreviousRowSpacingScale);
		}
		PreviousRowSpacingScale = SpacingScale;

		PlaceRow(RowRadii, DragPath, RowDepth, SpacingScale, OutPositions, OutRotations);
	}
}
