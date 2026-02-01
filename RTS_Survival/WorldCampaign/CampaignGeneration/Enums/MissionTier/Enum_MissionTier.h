#pragma once

#include "CoreMinimal.h"

#include "Enum_MissionTier.generated.h"

/**
 * @brief Mission tier identifiers used during MissionsPlaced to select tier rules
 *        after NeutralObjectsPlaced and before Finished.
 */
UENUM(BlueprintType)
enum class EMissionTier : uint8
{
	/** Unspecified tier; use per-mission overrides or defaults. */
	NotSet,
	/** Tier 1 missions, typically placed closest to the player HQ. */
	Tier1,
	/** Tier 2 missions, typically placed in mid-range regions. */
	Tier2,
	/** Tier 3 missions, typically placed in late-game regions. */
	Tier3,
	/** Tier 4 missions, typically placed farthest from the HQ. */
	Tier4
};
