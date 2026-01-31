#pragma once

#include "CoreMinimal.h"

#include "Enum_EnemyTopologySearchStrategy.generated.h"

UENUM(BlueprintType)
enum class EEnemyTopologySearchStrategy : uint8
{
	None,
	PreferLowDegree,     // prefer low degree of connections
	PreferHighDegree,    // prefer high degree of connections
	PreferChokepoints,   // high chokepoint score
	PreferDeadEnds,      // degree 1
	PreferNearMinBound,  // bias toward MinHops
	PreferNearMaxBound   // bias toward MaxHops
};
