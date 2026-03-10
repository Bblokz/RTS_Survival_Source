#pragma once

#include "CoreMinimal.h"
#include "TowAbilityTypes.generated.h"

UENUM(BlueprintType)
enum class ETowActorAbilitySubtypes : uint8
{
	None,
	TowVehicle,
	TowTeamWeapon,
};

UENUM(BlueprintType)
enum class ETowType : uint8
{
	None,
	ClickedTowVehicle,
	ClickedTeamWeaponToTow,
	ClickedVehicleToTow,
	DetachTow,
};
