#pragma once

#include "CoreMinimal.h"
#include "TowAbilityTypes.generated.h"

UENUM()
enum ETowAbilityType
{
	// Displays the ability as if owned by a team weapon; select tow vehicle to attach team weapon to.
	DefaultTeamWeapon,
	// Displays the ability as if owned by a vehicle; select towable actor to attach to vehicle.
	VehicleHook,
};

UENUM(BlueprintType)
enum class ETowedActorTarget : uint8
{
	None,
	TowVehicle,
	TowTeamWeapon,
};

UENUM(BlueprintType)
enum class ETowOrderType : uint8
{
	None,
	ClickedTowVehicle,
	ClickedTeamWeaponToTow,
	ClickedVehicleToTow,
	DetachTow,
};
