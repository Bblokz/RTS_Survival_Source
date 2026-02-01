#pragma once

#include "CoreMinimal.h"
#include "Enum_MapItemType.generated.h"

/**
 * @brief High-level map item categories used by adjacency rules during MissionsPlaced,
 *        which occurs after NeutralObjectsPlaced and before Finished.
 */
UENUM(BlueprintType)
enum class EMapItemType : uint8
{
	/** Undefined type used when no adjacency constraint is needed. */
	None,
	/** Empty anchor used to indicate no item is present. */
	Empty,
	/** Mission item category used when a mission must be adjacent. */
	Mission,
	/** Enemy item category used to require nearby enemy structures. */
	EnemyItem,
	/** Player item category used to require proximity to player infrastructure. */
	PlayerItem,
	/** Neutral item category used to require nearby neutral resources or landmarks. */
	NeutralItem
};
