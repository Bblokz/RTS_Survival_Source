#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderTypes.h"

class ASquadUnit;

/** What a soldier does at the spot the player's squad move preview gave it. */
enum class ESquadPlannedPositionType : uint8
{
	None,
	RegularStanding,
	StandingCover,
	CrouchCover
};

/** What happened when a soldier was told to start walking to its planned position. */
enum class ESquadPlannedMoveStart : uint8
{
	Walking,
	AlreadyThere,
	// No path, or the soldier cannot start walking this frame (a root-motion cover clip is still playing out).
	Failed
};

/**
 * @brief One soldier's destination from the squads-only move preview.
 * The same value is drawn in the preview, sent with the move order, and kept on the unit, so the automatic cover
 * layer ends up using exactly the spot the player was shown.
 */
struct FSquadUnitPlannedPosition
{
	// Where the soldier ends up: the cover point itself for cover, otherwise the formation slot.
	FVector Location = FVector::ZeroVector;

	// Horizontal direction the soldier faces there; toward the cover surface for cover positions.
	FVector Facing = FVector::ForwardVector;

	ESquadPlannedPositionType Type = ESquadPlannedPositionType::None;

	// Only meaningful for the two cover types.
	FRTSCoverPoint CoverPoint;

	bool GetIsCover() const
	{
		return Type == ESquadPlannedPositionType::StandingCover || Type == ESquadPlannedPositionType::CrouchCover;
	}
};

/** Pairs a planned position with the soldier it was planned for when it travels with a squad move order. */
struct FSquadUnitPlannedDestination
{
	TWeakObjectPtr<ASquadUnit> SquadUnit;
	FSquadUnitPlannedPosition Position;
};
