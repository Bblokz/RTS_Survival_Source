#pragma once

#include "CoreMinimal.h"


UENUM(Blueprintable)
enum class EPlayerError : uint8
{
	Error_None,
	Error_NotEnoughRadixite,
	Error_NotEnoughMetal,
	Error_NotEnoughEnergy,
	Error_NotEnoughVehicleParts,
	Error_NotEnoughtFuel,
	Error_NotENoughAmmo,
	Error_NotEnoughWeaponBlueprints,
	Error_NotEnoughBuildingBlueprints,
	Error_NotEnoughEnergyBlueprints,
	Error_NotEnoughVehicleBlueprints,
	Error_NotEnoughConstructionBlueprints,
	Error_LocationNotReachable,
};