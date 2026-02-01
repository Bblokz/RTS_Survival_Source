#pragma once

#include "CoreMinimal.h"

#include "Enum_EnemyTopologySearchStrategy.generated.h"

/**
 * @brief Enemy-specific topology biases used during EnemyObjectsPlaced, which happens
 *        after EnemyHQPlaced and before NeutralObjectsPlaced.
 */
UENUM(BlueprintType)
enum class EEnemyTopologySearchStrategy : uint8
{
	/** No preference; use raw candidate ordering. */
	None,
	/** Prefer anchors with fewer connections. */
	PreferLowDegree,     // prefer low degree of connections
	/** Prefer anchors with more connections. */
	PreferHighDegree,    // prefer high degree of connections
	/** Prefer anchors with high chokepoint scores. */
	PreferChokepoints,   // high chokepoint score
	/** Prefer degree-1 anchors to create dead-end placements. */
	PreferDeadEnds,      // degree 1
	/** Prefer anchors near the minimum hop bound. */
	PreferNearMinBound,  // bias toward MinHops
	/** Prefer anchors near the maximum hop bound. */
	PreferNearMaxBound   // bias toward MaxHops
};
