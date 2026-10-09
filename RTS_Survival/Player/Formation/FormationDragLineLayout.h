#pragma once

#include "CoreMinimal.h"

struct FFormationDragPath;

/**
 * @brief Pure layout of units along a dragged line, working on formation radii only so it can be tested
 * without units. The formation controller asks for the row sizes, orders its units inside each row and
 * then asks for the slot of every unit.
 */
struct RTS_SURVIVAL_API FFormationDragLineLayout
{
	/**
	 * @brief Fills rows front to back with as many units as fit shoulder to shoulder on the line.
	 * @param UnitRadii  Formation radius of every unit, in the order the rows are filled.
	 * @param LineLength Length of the dragged line.
	 * @return Number of units in each row; every row holds at least one unit.
	 */
	static TArray<int32> ComputeRowUnitCounts(const TArray<float>& UnitRadii, float LineLength);

	/**
	 * @brief Places every row along the line: stretched to the line's ends, with later rows behind it.
	 * @param RowUnitRadii Formation radii per row, each row ordered from the start of the line to its end.
	 * @param DragPath     The dragged line.
	 * @param OutPositions Receives one position per unit, row after row.
	 * @param OutRotations Receives the facing of each position, parallel to OutPositions.
	 */
	static void PlaceRows(
		const TArray<TArray<float>>& RowUnitRadii,
		const FFormationDragPath& DragPath,
		TArray<FVector>& OutPositions,
		TArray<FRotator>& OutRotations);

	/** Distance between the centres of two neighbouring units standing shoulder to shoulder. */
	static float GetNeighbourSpacing(float FirstUnitRadius, float SecondUnitRadius);
};
