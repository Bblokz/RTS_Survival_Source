#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderTypes.h"
#include "RTS_Survival/Player/Formation/FormationTypes.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnitPlannedPosition.h"

/** Distances that shape a squad move plan; one copy is shared by the preview and the issued order. */
struct FSquadMovePlannerSettings
{
	// Shoulder-to-shoulder distance between soldiers that stand in the open.
	float RegularUnitSpacing = 130.0f;
	float RegularRowSpacing = 150.0f;
	int32 MaximumRegularUnitsPerRow = 5;

	// A squad in the open forms a block this many times wider than deep, measured in soldiers.
	float RegularBlockWidthToDepthRatio = 1.5f;

	// Free ground between the blocks of neighbouring squads, sideways and front to back.
	float SquadGap = 220.0f;

	// Widest row of squads in the spear shapes; the same numbers the mixed formation uses for its units.
	int32 SpearSquadsPerRow = 5;
	int32 ThinSpearSquadsPerRow = 3;

	// The semi circle starts with this many squads and adds two per arc behind it.
	int32 SemiCircleFirstArcSquads = 3;
	// Each squad further from the middle of an arc turns this much further outward.
	float SemiCircleFanDegreesPerSquad = 20.0f;
	float MaximumSemiCircleFanDegrees = 60.0f;

	// Two planned positions are never closer than this, whatever their type.
	float MinimumSlotSpacing = 75.0f;

	// Cover further from a squad's anchor than this is ignored; it grows with the squad so every soldier can fit.
	float CoverSnapBaseRadius = 400.0f;
	float CoverSnapRadiusPerUnit = 50.0f;
	float MaximumCoverSnapRadius = 800.0f;

	// With a facing chosen by the player, cover must protect against that direction by at least this dot product.
	float MinimumChosenFacingAlignment = 0.2f;

	// Extra distance charged to cover that faces away from the squad's facing, so near-side cover wins ties.
	float FacingMisalignmentPenalty = 200.0f;

	// Distance credited to cover already used by the previous plan, so small mouse moves do not reshuffle soldiers.
	float PreviousPlanBonus = 60.0f;

	// Soldiers without cover line up this far behind the ones that found some.
	float RegularBehindCoverDistance = 180.0f;
};

/** One selected soldier; its index in the request is its index in the resulting plan. */
struct FSquadMovePlannerUnit
{
	int32 SquadIndex = 0;
	FVector Location = FVector::ZeroVector;
};

/** Everything the planner needs, copied out of the world so planning itself touches no actors. */
struct FSquadMovePlanRequest
{
	// Ground location under the cursor, or where the rotation arrow was started.
	FVector Anchor = FVector::ZeroVector;

	// Direction the squads should face; the arrow's direction when the player dragged it.
	FVector Facing = FVector::ForwardVector;
	bool bFacingChosenByPlayer = false;

	// Shape picked in the formation picker; decides where the squads stand relative to each other.
	EFormation Formation = EFormation::RectangleFormation;

	int32 SquadCount = 0;
	TArray<FSquadMovePlannerUnit> Units;

	// Published cover near the anchor that no soldier outside the selection has reserved.
	TArray<FRTSCoverPoint> CoverPoints;

	// Cover used by the plan currently on screen.
	TSet<int64> PreviousCoverPointIds;

	FSquadMovePlannerSettings Settings;
};

/** Result of planning: one position per requested soldier, and one anchor and facing per squad. */
struct FSquadMovePlan
{
	TArray<FSquadUnitPlannedPosition> UnitPositions;
	TArray<FVector> SquadAnchors;
	TArray<FVector> SquadFacings;

	void Reset()
	{
		UnitPositions.Reset();
		SquadAnchors.Reset();
		SquadFacings.Reset();
	}
};

/**
 * @brief Decides where every soldier of the selected squads goes for a cursor location and facing.
 * Pure value logic: the preview component feeds it copied data every time the cursor moves, and the same
 * call produces the destinations sent with the move order.
 */
struct FSquadMovePlanner
{
	/**
	 * @brief Builds positions that use nearby cover first and keep the rest of each squad together.
	 * @param Request Anchor, facing, soldiers and cover candidates.
	 * @param OutPlan One position per soldier in request order, plus the anchor of each squad.
	 */
	static void BuildPlan(const FSquadMovePlanRequest& Request, FSquadMovePlan& OutPlan);

	/** @return Widest distance from the cursor at which any squad of this request may still use cover. */
	static float GetCoverQueryRadius(const FSquadMovePlanRequest& Request);
};
