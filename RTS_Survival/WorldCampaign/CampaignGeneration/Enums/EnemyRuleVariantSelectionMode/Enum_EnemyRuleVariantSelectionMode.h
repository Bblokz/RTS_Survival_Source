#pragma once

#include "CoreMinimal.h"

#include "Enum_EnemyRuleVariantSelectionMode.generated.h"

/**
 * @brief Variant selection modes used during EnemyObjectsPlaced, which follows
 *        EnemyHQPlaced and precedes NeutralObjectsPlaced.
 */
UENUM(BlueprintType)
enum class EEnemyRuleVariantSelectionMode : uint8
{
	/** No variants are used; only base rules apply. */
	None,
	/** Cycle variants by placement index to produce a repeating pattern. */
	CycleByPlacementIndex   // VariantIndex = InstanceIndex % Variants.Num()
};
