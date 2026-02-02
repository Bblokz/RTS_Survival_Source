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
	/** Early-game mission that is added after HQ placement; used to introduce basic mechanics. */
	FirstCampaignClearRoad,
	/** The second mision that will be started at the HQ location of the player */
	SecondCampaignBuildBase,
	/** In this mission the player must choose to either defend the marder or the stug factory hillside */
	MarderStugMission,
	/** Hetzer mission placement target used for mid-campaign armored encounters. */
	HetzerMission,
	/** This mission provides the player with a small amount of new halftracks and infantry for a forrest mission */
	HalftrackForrestMission,
	/** in this mission the player must destroy a soviet T-35 patrol guarding their mining base */
	T35MiningBase,
	/** in this mission the player unlocks either the panzer IV or panzer III M and is tasked to destroy a platoon of soviet armor */
	PanzerIV_IIIM_Mission,
	/** Unlocks special flame thrower units in a mission to destroy the soviet fuel depots. */
	FlameThrowerFuelDepots,
	/** In this mission the player unlocks the ME410 ground attack aircraft */
	ME410UnlockMission,
	/** Tiger/Panther range mission used to position late-game armored threats. */
	TigerPantherRange,
	/** Jagdpanther or Jagdtiger mission used to drive heavy-hunter objectives. */
	JagdpantherMission,
	/** THis mission unlocks the KingTiger */
	KingTigerMission_SovietWall,
	/** THis mission unlocks the Maus or E100*/
	Maus_E100_AssaultMission_Moscow,
	/** Sturmtiger dam mission where the player unlocks the sturm tiger to blow up a dam. */
	SturmtigerDam
};
