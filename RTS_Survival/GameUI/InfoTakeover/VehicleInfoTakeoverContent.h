// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "InfoTakeoverTypes.h"

class AActor;
class ATankMaster;
class UDataTable;

/**
 * Builds what a vehicle info takeover shows from a library entry and the live vehicle: the entry's text and picture,
 * and the weapons and armour the vehicle carries now. Used by UInfoTakeoverSubsystem when a mission node is called.
 */
namespace VehicleInfoTakeoverContent
{
	/**
	 * @brief The entry's hand-written parts with the unit's training title and picture where the entry leaves them
	 * empty, plus the vehicle's weapons and armour.
	 * @param Vehicle The live vehicle; its BeginPlay weapon and armour setup must have run.
	 * @param Entry The library prefab or the mission node's own parameters.
	 * @param TrainingOptionsTable DT_TrainingOptions, for the default title and picture; may be null.
	 * @return The content the widget shows.
	 */
	FVehicleInfoTakeoverContent BuildContent(const ATankMaster& Vehicle, const FVehicleInfoTakeoverEntry& Entry,
	                                         const UDataTable* TrainingOptionsTable);

	/** @return Front range and the thinnest side and rear plates of the vehicle's registered armour. */
	FInfoTakeoverArmorSummary ReadArmorSummary(const AActor& Vehicle);
}
