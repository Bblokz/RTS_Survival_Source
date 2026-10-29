// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitVariantStatComparison.h"

#include "UnitVariantProbes.h"

namespace UnitVariantStatComparisonPrivate
{
	// Logarithmic, because the game's values span orders of magnitude (a 9 damage MG next to a 2220 damage mortar):
	// on a linear scale every gun but the biggest would show one pip. Bounds cover every German weapon.
	constexpr float MinDamage = 8.f;
	constexpr float MaxDamage = 2000.f;
	constexpr float MinArmorPen = 10.f;
	constexpr float MaxArmorPen = 800.f;
	constexpr float FastestReloadSeconds = 1.f;
	constexpr float SlowestReloadSeconds = 45.f;
	// Health and speed changes smaller than this are noise, not a trade-off worth a row.
	constexpr float MinimumChangePercent = 3.f;
	constexpr float ChangeStepPercent = 15.f;
	constexpr float PercentScale = 100.f;
	// Weapons further apart in calibre than this are different roles, not a replacement of each other.
	constexpr float MaxComparableCalibreRatio = 2.f;
	constexpr float MinimumTrendPercent = 1.f;

	/** @return 1 at Min (or below), 10 at Max (or above), logarithmic in between. */
	int32 GetLogLevel(const float Value, const float Min, const float Max)
	{
		using namespace UnitVariantStatComparison;
		if (Value <= Min)
		{
			return 1;
		}
		const float Fraction = FMath::Loge(Value / Min) / FMath::Loge(Max / Min);
		return FMath::Clamp(1 + FMath::RoundToInt(Fraction * (MaxStatLevel - 1)), 1, MaxStatLevel);
	}

	void AddRowIfChanged(TArray<FUnitVariantStatRow>& Rows, const FText& Label, const float VariantValue,
	                     const float BaseValue)
	{
		using namespace UnitVariantStatComparison;
		if (BaseValue <= 0.f)
		{
			return;
		}
		const float ChangePercent = (VariantValue - BaseValue) / BaseValue * PercentScale;
		if (FMath::Abs(ChangePercent) < MinimumChangePercent)
		{
			return;
		}
		FUnitVariantStatRow& Row = Rows.AddDefaulted_GetRef();
		Row.Label = Label;
		Row.Steps = FMath::Clamp(FMath::CeilToInt(FMath::Abs(ChangePercent) / ChangeStepPercent), 1, MaxChangeSteps);
		Row.bIsIncrease = ChangePercent > 0.f;
		// More health and more speed are both better.
		Row.bIsBetter = Row.bIsIncrease;
	}
}

int32 UnitVariantStatComparison::GetDamageLevel(const float Damage)
{
	using namespace UnitVariantStatComparisonPrivate;
	return GetLogLevel(Damage, MinDamage, MaxDamage);
}

int32 UnitVariantStatComparison::GetPenetrationLevel(const float ArmorPen)
{
	using namespace UnitVariantStatComparisonPrivate;
	return GetLogLevel(ArmorPen, MinArmorPen, MaxArmorPen);
}

int32 UnitVariantStatComparison::GetReloadLevel(const float ReloadSeconds)
{
	using namespace UnitVariantStatComparisonPrivate;
	// Inverted: the slowest reload is level 1, the fastest level 10.
	const int32 SlownessLevel = GetLogLevel(ReloadSeconds, FastestReloadSeconds, SlowestReloadSeconds);
	return MaxStatLevel + 1 - SlownessLevel;
}

const FUnitVariantWeaponStats* UnitVariantStatComparison::FindMainGun(const FUnitVariantProbeResult& Result)
{
	const FUnitVariantWeaponStats* MainGun = nullptr;
	for (const FUnitVariantWeaponStats& Weapon : Result.Weapons)
	{
		const bool bIsLarger = MainGun == nullptr || Weapon.CalibreMm > MainGun->CalibreMm
			|| (Weapon.CalibreMm == MainGun->CalibreMm && Weapon.Damage > MainGun->Damage);
		if (bIsLarger)
		{
			MainGun = &Weapon;
		}
	}
	return MainGun;
}

const FUnitVariantWeaponStats* UnitVariantStatComparison::FindComparableBaseWeapon(
	const FUnitVariantWeaponStats& Weapon, const FUnitVariantProbeResult& Variant, const FUnitVariantProbeResult& Base)
{
	using namespace UnitVariantStatComparisonPrivate;
	if (const FUnitVariantWeaponStats* const SameWeapon = Base.Weapons.FindByPredicate(
		[&Weapon](const FUnitVariantWeaponStats& BaseWeapon) { return BaseWeapon.WeaponRowName == Weapon.WeaponRowName; }))
	{
		return SameWeapon;
	}
	// The main gun is what a variant swaps; judge it against what it replaced even across calibres.
	if (FindMainGun(Variant) == &Weapon)
	{
		return FindMainGun(Base);
	}
	const FUnitVariantWeaponStats* Closest = nullptr;
	float ClosestRatio = MaxComparableCalibreRatio;
	for (const FUnitVariantWeaponStats& BaseWeapon : Base.Weapons)
	{
		if (BaseWeapon.WeaponRowName == Weapon.WeaponRowName)
		{
			return &BaseWeapon;
		}
		if (Weapon.CalibreMm <= 0.f || BaseWeapon.CalibreMm <= 0.f)
		{
			continue;
		}
		const float Ratio = FMath::Max(Weapon.CalibreMm, BaseWeapon.CalibreMm)
			/ FMath::Min(Weapon.CalibreMm, BaseWeapon.CalibreMm);
		if (Ratio <= ClosestRatio)
		{
			ClosestRatio = Ratio;
			Closest = &BaseWeapon;
		}
	}
	return Closest;
}

EUnitVariantStatTrend UnitVariantStatComparison::GetTrend(const float VariantValue, const float BaseValue,
                                                          const bool bHigherIsBetter)
{
	using namespace UnitVariantStatComparisonPrivate;
	if (BaseValue <= 0.f)
	{
		return EUnitVariantStatTrend::Same;
	}
	const float ChangePercent = (VariantValue - BaseValue) / BaseValue * PercentScale;
	if (FMath::Abs(ChangePercent) < MinimumTrendPercent)
	{
		return EUnitVariantStatTrend::Same;
	}
	return (ChangePercent > 0.f) == bHigherIsBetter ? EUnitVariantStatTrend::Better : EUnitVariantStatTrend::Worse;
}

TArray<FUnitVariantStatRow> UnitVariantStatComparison::CompareWithBase(const FUnitVariantProbeResult& Variant,
                                                                        const FUnitVariantProbeResult& Base)
{
	using namespace UnitVariantStatComparisonPrivate;
	TArray<FUnitVariantStatRow> Rows;
	AddRowIfChanged(Rows, NSLOCTEXT("UnitVariantSelection", "StatHealth", "Health"), Variant.MaxHealth,
	                Base.MaxHealth);
	AddRowIfChanged(Rows, NSLOCTEXT("UnitVariantSelection", "StatSpeed", "Speed"), Variant.MaxSpeedMultiplier,
	                Base.MaxSpeedMultiplier);
	return Rows;
}
