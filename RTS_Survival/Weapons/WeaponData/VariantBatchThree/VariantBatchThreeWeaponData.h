// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/AttachedRocketsData/AttachedRocketsData.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/RocketAbilityTypes.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

/**
 * Unit variant batch 3 weapon and rocket data, registered by ACPPGameState after every category initializer so the
 * clones can copy their source entries. All numbers are TUNABLE (the user's balance call); stored values are before the
 * shell multipliers that GLOBAL_GetWeaponDataForShellType applies when firing.
 */
namespace VariantBatchThreeWeapons
{
	// The Puma Radixite Sniper's damage is per laser iteration; its turret BP must use this many BeamIterations.
	constexpr int32 RadixiteSniperBeamIterations = 1;
	// The sniper deals this many base Puma (KwK 39/1) shots in one beam.
	constexpr float RadixiteSniperDamageInPumaShots = 7.f;

	/** @param WeaponDataMap The player weapon map, already filled by the category initializers. */
	void AddWeapons(TMap<EWeaponName, FWeaponData>& WeaponDataMap);

	/** @param RocketDataMap The player attached-rocket map; the enemy map is copied from it afterwards. */
	void AddRocketData(TMap<ERocketAbility, FAttachedRocketsData>& RocketDataMap);
}
