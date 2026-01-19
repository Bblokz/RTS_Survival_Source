// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehWeaponChangeFlameColor.h"

#include "RTS_Survival/Weapons/FlameThrowerWeapon/UWeaponStateFlameThrower.h"

UBehWeaponChangeFlameColor::UBehWeaponChangeFlameColor() = default;

bool UBehWeaponChangeFlameColor::CheckRequirement(UWeaponState* WeaponState) const
{
	if (WeaponState == nullptr)
	{
		return false;
	}

	if (not Super::CheckRequirement(WeaponState))
	{
		return false;
	}

	return WeaponState->IsA(UWeaponStateFlameThrower::StaticClass());
}

void UBehWeaponChangeFlameColor::ApplyBehaviourToWeapon(UWeaponState* WeaponState)
{
	Super::ApplyBehaviourToWeapon(WeaponState);

	UWeaponStateFlameThrower* FlameThrowerWeapon = Cast<UWeaponStateFlameThrower>(WeaponState);
	if (not FlameThrowerWeapon)
	{
		return;
	}

	ApplyColorOverride(FlameThrowerWeapon);
}

void UBehWeaponChangeFlameColor::RemoveBehaviourFromWeapon(UWeaponState* WeaponState)
{
	UWeaponStateFlameThrower* FlameThrowerWeapon = Cast<UWeaponStateFlameThrower>(WeaponState);
	if (not FlameThrowerWeapon)
	{
		Super::RemoveBehaviourFromWeapon(WeaponState);
		return;
	}

	RestoreColorOverride(FlameThrowerWeapon);
	Super::RemoveBehaviourFromWeapon(WeaponState);
}

void UBehWeaponChangeFlameColor::ApplyColorOverride(UWeaponStateFlameThrower* FlameThrowerWeapon)
{
	if (not FlameThrowerWeapon)
	{
		return;
	}

	FFlameThrowerSettings* FlameSettings = FlameThrowerWeapon->GetFlameSettingsForChange();
	if (FlameSettings == nullptr)
	{
		return;
	}

	const TWeakObjectPtr<UWeaponStateFlameThrower> WeaponPtr(FlameThrowerWeapon);
	if (not M_PreviousFlameColors.Contains(WeaponPtr))
	{
		M_PreviousFlameColors.Add(WeaponPtr, FlameSettings->Color);
	}

	FlameSettings->Color = M_FlameColorOverride;
	FlameThrowerWeapon->ApplyFlameColorFromSettings();
}

void UBehWeaponChangeFlameColor::RestoreColorOverride(UWeaponStateFlameThrower* FlameThrowerWeapon)
{
	if (not FlameThrowerWeapon)
	{
		return;
	}

	const TWeakObjectPtr<UWeaponStateFlameThrower> WeaponPtr(FlameThrowerWeapon);
	FLinearColor PreviousColor = FLinearColor::White;
	if (not TryGetCachedColor(WeaponPtr, PreviousColor))
	{
		return;
	}

	FFlameThrowerSettings* FlameSettings = FlameThrowerWeapon->GetFlameSettingsForChange();
	if (FlameSettings == nullptr)
	{
		return;
	}

	FlameSettings->Color = PreviousColor;
	FlameThrowerWeapon->ApplyFlameColorFromSettings();
	M_PreviousFlameColors.Remove(WeaponPtr);
}

bool UBehWeaponChangeFlameColor::TryGetCachedColor(
	const TWeakObjectPtr<UWeaponStateFlameThrower>& WeaponPtr,
	FLinearColor& OutColor) const
{
	const FLinearColor* CachedColor = M_PreviousFlameColors.Find(WeaponPtr);
	if (CachedColor == nullptr)
	{
		return false;
	}

	OutColor = *CachedColor;
	return true;
}
