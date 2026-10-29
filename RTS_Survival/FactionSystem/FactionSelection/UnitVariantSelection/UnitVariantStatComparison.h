// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FUnitVariantProbeResult;
struct FUnitVariantWeaponStats;

/** How one stat of a variant's weapon compares with the matching weapon of Base; colours its pips. */
enum class EUnitVariantStatTrend : uint8
{
	// Equal, Base itself, or a weapon Base has nothing comparable to.
	Same,
	Better,
	Worse
};

/** One unit-wide stat the variant changes against Base, shown as up or down arrows instead of a number. */
struct FUnitVariantStatRow
{
	FText Label;
	// 1 to MaxChangeSteps arrows, one per ChangeStepPercent of change.
	int32 Steps = 1;
	bool bIsIncrease = false;
	bool bIsBetter = false;
};

/**
 * Turns probe results into what the detail panel draws: weapon stats as 1-10 levels on one fixed scale for every
 * unit (so a 10 means the same everywhere), and the unit-wide stats a variant changes against Base. Players read
 * filled pips and arrows instead of damage numbers and percentages.
 */
namespace UnitVariantStatComparison
{
	constexpr int32 MaxStatLevel = 10;
	constexpr int32 MaxChangeSteps = 3;

	/** @return 1-10: machine guns sit at the bottom, heavy howitzers at the top. */
	int32 GetDamageLevel(const float Damage);

	/** @return 1-10 for the armour penetration. */
	int32 GetPenetrationLevel(const float ArmorPen);

	/** @return 1-10 where a shorter reload fills more pips. */
	int32 GetReloadLevel(const float ReloadSeconds);

	/** @return 1-10 for the rounds per magazine: single-shot cannons at 1, belt-fed machine guns near 10. */
	int32 GetMagazineLevel(const int32 MagCapacity);

	/**
	 * @brief The Base weapon a variant's weapon is judged against: the same weapon; for the variant's main gun Base's
	 * main gun (the Maus AA's 88 mm Flak against the 128 mm it replaces); else Base's weapon of the closest calibre
	 * within a factor of two.
	 * @param Weapon A weapon of the variant.
	 * @param Variant The variant's probe result, to know its main gun.
	 * @param Base Base's probe result.
	 * @return Null when Base has nothing comparable (an added machine gun next to a howitzer).
	 */
	const FUnitVariantWeaponStats* FindComparableBaseWeapon(const FUnitVariantWeaponStats& Weapon,
	                                                        const FUnitVariantProbeResult& Variant,
	                                                        const FUnitVariantProbeResult& Base);

	/** @return The largest calibre weapon (highest damage on a tie), null when the unit has none. */
	const FUnitVariantWeaponStats* FindMainGun(const FUnitVariantProbeResult& Result);

	/** @return Same when the values differ by less than a percent. */
	EUnitVariantStatTrend GetTrend(const float VariantValue, const float BaseValue, const bool bHigherIsBetter);

	/** @return Health and speed when they differ from Base by at least a few percent; empty when nothing changed. */
	TArray<FUnitVariantStatRow> CompareWithBase(const FUnitVariantProbeResult& Variant,
	                                            const FUnitVariantProbeResult& Base);
}
