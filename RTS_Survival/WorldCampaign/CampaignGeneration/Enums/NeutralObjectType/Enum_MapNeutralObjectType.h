#pragma once

#include "CoreMinimal.h"

#include "Enum_MapNeutralObjectType.generated.h"

/**
 * @brief Neutral object identifiers used during NeutralObjectsPlaced, which occurs after
 *        EnemyObjectsPlaced and before MissionsPlaced.
 */
UENUM(BlueprintType)
enum class EMapNeutralObjectType : uint8
{
	/** No neutral object assigned; treated as empty during neutral placement. */
	None,
	/** Radixite field resource node used to seed harvesting routes. */
	RadixiteField,
	/** Ruined city landmark used for scavenging or mission adjacency. */
	RuinedCity,
	/** Dense forest landmark used to influence mission adjacency or route flavor. */
	DenseForrest
};
