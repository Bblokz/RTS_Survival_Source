#pragma once

#include "CoreMinimal.h"
#include "Enum_MapPlayerItem.generated.h"

/**
 * @brief Player-controlled item identifiers used when player-side map objects are placed
 *        after PlayerHQPlaced and before EnemyHQPlaced in the generation flow.
 */
UENUM(BlueprintType)
enum class EMapPlayerItem : uint8
{
	/** No player item assigned; treated as empty for player-side placement. */
	None,
	/** The main player headquarters, placed during the PlayerHQPlaced step. */
	PlayerHQ,
	/** Player factory node used to extend production access from the HQ. */
	Factory,
	/** Player airfield node used to open air routes from the starting region. */
	Airfield,
	/** Player barracks node used to reinforce early ground expansion. */
	Barracks
};
