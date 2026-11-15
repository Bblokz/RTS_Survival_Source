// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "InfoTakeoverTypes.h"

class AActor;
class ASquadUnit;
class UDataTable;

/**
 * Builds what a squad info takeover shows from a library entry and the live infantry: the entry's text and picture,
 * and the soldiers, their health and their weapons now. Used by UInfoTakeoverSubsystem when a mission node is called.
 */
namespace SquadInfoTakeoverContent
{
	/**
	 * @brief The entry's hand-written parts with the unit's training title and picture where the entry leaves them
	 * empty, plus what the soldiers carry.
	 * @param Infantry A squad controller (all its living soldiers) or a solo infantry unit (itself).
	 * @param Entry The library prefab or the mission node's own parameters.
	 * @param TrainingOptionsTable DT_TrainingOptions, for the default title and picture; may be null.
	 * @return The content the widget shows.
	 */
	FSquadInfoTakeoverContent BuildContent(AActor& Infantry, const FSquadInfoTakeoverEntry& Entry,
	                                       const UDataTable* TrainingOptionsTable);

	/** @return The living soldiers of a squad controller, or the unit itself for a squad unit; empty otherwise. */
	TArray<ASquadUnit*> GetSoldiers(AActor& Infantry);

	/** @return True for the actors the squad nodes accept: a squad controller or a (solo) squad unit. */
	bool GetIsInfantry(const AActor& Actor);
}
