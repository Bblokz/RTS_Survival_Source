// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

/**
 * Unit variant batch 4 weapon data (Wespe, Pz III M, Hetzer, JagdPanther), registered by ACPPGameState after batch 3
 * so the clones can copy their source entries. All numbers are TUNABLE (the user's balance call); stored values are
 * before the shell multipliers that GLOBAL_GetWeaponDataForShellType applies when firing.
 */
namespace VariantBatchFourWeapons
{
	// Wespe Radixite Howitzer: the fired radixite shell hits this many times as hard as the base Wespe's fired HE.
	constexpr float RadixiteHowitzerDamageInBaseShells = 1.5f;
	constexpr float RadixiteHowitzerCalibre = 200.f;
	constexpr float RadixiteHowitzerDotTotalDamage = 60.f;
	constexpr float RadixiteHowitzerDotDurationSeconds = 6.f;

	// Pz III M Overcharged: one laser shot deals this many base Pz III M (KwK 39/1) shots, reload 4x the KwK 39/1's.
	// The damage is per beam iteration; the turret BP must use this many BeamIterations.
	constexpr int32 PzIIIMLaserBeamIterations = 1;
	constexpr float PzIIIMLaserDamageInKwK39Shots = 3.f;
	constexpr float PzIIIMLaserReloadInKwK39Reloads = 4.f;

	// Hetzer Flame: the Flamm 15 with 33% less damage.
	constexpr float HetzerHullFlameDamage = 16.75f;

	// Hetzer Mortar (user H1 defaults) relative to the base 12 cm mortar.
	constexpr float HetzerMortarDamageMlt = 1.5f;
	constexpr float HetzerMortarTntMlt = 1.4f;
	constexpr float HetzerMortarReloadSpeed = 7.f;

	// JagdPanther Pfeil (J-P3 defaults).
	constexpr float PfeilDamagePerCalibreMlt = 1.15f;
	constexpr float PfeilArmorPen = 255.f;
	constexpr float PfeilArmorPenMaxRange = 250.f;
	constexpr float PfeilRangeMlt = 1.1f;
	constexpr int32 PfeilAccuracy = 97;

	// JagdPanther Bollwerk (J-B2 defaults).
	constexpr float BollwerkReloadSpeed = 16.f;
	constexpr int32 BollwerkAccuracy = 82;

	/** @param WeaponDataMap The player weapon map, already filled by the category initializers and batches 2-3. */
	void AddWeapons(TMap<EWeaponName, FWeaponData>& WeaponDataMap);
}
