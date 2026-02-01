#pragma once

#include "CoreMinimal.h"

#include "Enum_PlacementFailurePolicy.generated.h"

/**
 * @brief Policies for handling step failures across placement steps such as
 *        ConnectionsCreated through MissionsPlaced.
 */
UENUM(BlueprintType)
enum class EPlacementFailurePolicy : uint8
{
	/** No policy specified; fall back to a global policy or report an error. */
	NotSet,
	/** Immediately backtrack to the previous step without loosening rules. */
	InstantBackTrack,
	/** Relax distance rules for the step, then backtrack and retry placement. */
	BreakDistanceRules_ThenBackTrack
};
