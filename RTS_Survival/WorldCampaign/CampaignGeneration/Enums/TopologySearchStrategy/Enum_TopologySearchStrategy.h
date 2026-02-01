#pragma once

#include "CoreMinimal.h"

#include "Enum_TopologySearchStrategy.generated.h"

/**
 * @brief Generic topology bias used across placement rules to favor min or max values
 *        during steps like PlayerHQPlaced, EnemyHQPlaced, NeutralObjectsPlaced, and MissionsPlaced.
 */
UENUM(BlueprintType)
enum class ETopologySearchStrategy : uint8
{
	/** No preference; use raw candidate ordering. */
	NotSet,
	/** Prefer maximum values (e.g., farthest distance or highest degree). */
	PreferMax,
	/** Prefer minimum values (e.g., closest distance or lowest degree). */
	PreferMin
};
