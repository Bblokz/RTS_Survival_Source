#pragma once

#include "CoreMinimal.h"

#include "Enum_AdjacencyPolicy.generated.h"

/**
 * @brief Policies for adjacency requirements during MissionsPlaced, which occurs after
 *        NeutralObjectsPlaced and before Finished.
 */
UENUM(BlueprintType)
enum class EAdjacencyPolicy : uint8
{
	/** No policy specified; adjacency requirements will not be enforced. */
	NotSet,
	/** Reject candidate anchors unless the requirement is already satisfied. */
	RejectIfMissing,         // mission candidate anchor is invalid unless requirement already satisfied
	/** Try to auto-place the companion object if missing. */
	TryAutoPlaceCompanion    // if missing, try to place the required object on a connected yet empty anchor or
	                         // in case this about a neutral object and having a mission or map object spawned on top of it;
	                         // try spawn the neutral object on an yet empty anchor and place our object on top of that.
};
