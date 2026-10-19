#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EPlayerAimAbilityTypes : uint8
{
	None,
	AntiInfantryGrenade,
	AntiVehicleGrenade,
	SmallCrosshair,
	MediumCrosshair,
	LargeCrosshair,
	// Append only: abilities store these values in their Blueprint defaults.
	LargeComplexCrosshair
};