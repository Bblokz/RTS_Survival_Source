#pragma once

#include "CoreMinimal.h"
#include "Enum_MapEnemyItem.generated.h"

/**
 * @brief Enemy map item identifiers used during EnemyObjectsPlaced, which occurs after
 *        EnemyHQPlaced and before NeutralObjectsPlaced.
 */
UENUM(BlueprintType)
enum class EMapEnemyItem : uint8
{
	/** No enemy item assigned; treated as empty during enemy placement. */
	None,
	/** The main enemy headquarters, already placed in the EnemyHQPlaced step. */
	EnemyHQ,
	/** Enemy factory structure used to expand production routes after HQ placement. */
	Factory,
	/** Enemy airfield item typically placed to extend threat range mid-campaign. */
	Airfield,
	/** Enemy barracks item used to populate ground force hubs. */
	Barracks,
	/** Enemy supply depot item used to reinforce logistics nodes. */
	SupplyDepo,
	/** Enemy research facility item used to represent tech progression nodes. */
	ResearchFacility,
	/** Enemy fortified checkpoint item used to gate routes between regions. */
	FortifiedCheckpoint
};
