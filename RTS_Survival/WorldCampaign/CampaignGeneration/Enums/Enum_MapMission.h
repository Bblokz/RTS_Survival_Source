#pragma once

#include "CoreMinimal.h"
#include "Enum_MapMission.generated.h"

/**
 * @brief Mission identifiers used during MissionsPlaced, which happens after
 *        NeutralObjectsPlaced and before Finished.
 */
UENUM(BlueprintType)
enum class EMapMission : uint8
{
	/** No mission assigned; treated as empty during mission placement. */
	None,
	/** Hetzer mission placement target used for mid-campaign armored encounters. */
	HetzerMission,
	/** Tiger/Panther range mission used to position late-game armored threats. */
	TigerPantherRange,
	/** Jagdpanther mission used to drive heavy-hunter objectives. */
	JagdpantherMission,
	/** Sturmtiger dam mission used as a capstone or set-piece objective. */
	SturmtigerDam
};
