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

UENUM(BlueprintType)
enum class EGlobalAbilityAimShape : uint8
{
	// The crosshair picked by the aim type, placed with one click.
	Reticle,
	// A round area drawn by a custom shader, placed with one click.
	Circle,
	// A line drawn by a custom shader: the first click sets where the flight starts, the second where it ends.
	Strip
};