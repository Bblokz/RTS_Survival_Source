// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehWeaponOverwriteVFX.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

UBehWeaponOverwriteVFX::UBehWeaponOverwriteVFX() = default;

void UBehWeaponOverwriteVFX::ApplybehaviourToWeaponStandalone(UWeaponState* WeaponState)
{
	if (not IsValid(WeaponState))
	{
		return;
	}
	if (CheckRequirement(WeaponState))
	{
		ApplyBehaviourToWeapon(WeaponState);
	}
}

void UBehWeaponOverwriteVFX::RemovebehaviourFromWeaponStandalone(UWeaponState* WeaponState)
{
	if (not IsValid(WeaponState))
	{
		return;
	}
	if (CheckRequirement(WeaponState))
	{
		RemoveBehaviourFromWeapon(WeaponState);
	}
}

void UBehWeaponOverwriteVFX::ApplyBehaviourToWeapon(UWeaponState* WeaponState)
{
	if (WeaponState == nullptr)
	{
		return;
	}

	ApplyVfxOverrides(WeaponState);
	UBehaviourWeapon::ApplyBehaviourToWeapon(WeaponState);
}

void UBehWeaponOverwriteVFX::RemoveBehaviourFromWeapon(UWeaponState* WeaponState)
{
	if (WeaponState == nullptr)
	{
		return;
	}

	UBehaviourWeapon::RemoveBehaviourFromWeapon(WeaponState);
	RestoreVfxOverrides(WeaponState);
}

bool UBehWeaponOverwriteVFX::GetHasAnyVfxOverride() const
{
	return M_VfxOverrides.bOverrideLaunchSound
		|| M_VfxOverrides.bOverrideBounceSound
		|| M_VfxOverrides.bOverrideLaunchEffectSettings
		|| M_VfxOverrides.bOverrideSurfaceImpactEffects
		|| M_VfxOverrides.bOverrideShellSpecificVfxOverwrites
		|| M_VfxOverrides.bOverrideBounceEffect
		|| M_VfxOverrides.bOverrideLaunchScale
		|| M_VfxOverrides.bOverrideImpactScale
		|| M_VfxOverrides.bOverrideBounceScale
		|| M_VfxOverrides.bOverrideImpactAttenuation
		|| M_VfxOverrides.bOverrideImpactConcurrency
		|| M_VfxOverrides.bOverrideLaunchAttenuation
		|| M_VfxOverrides.bOverrideLaunchConcurrency;
}

FBehWeaponVfxOverrideFlags UBehWeaponOverwriteVFX::BuildOverrideFlags() const
{
	FBehWeaponVfxOverrideFlags Flags;
	Flags.bOverrideLaunchSound = M_VfxOverrides.bOverrideLaunchSound;
	Flags.bOverrideBounceSound = M_VfxOverrides.bOverrideBounceSound;
	Flags.bOverrideLaunchEffectSettings = M_VfxOverrides.bOverrideLaunchEffectSettings;
	Flags.bOverrideSurfaceImpactEffects = M_VfxOverrides.bOverrideSurfaceImpactEffects;
	Flags.bOverrideShellSpecificVfxOverwrites = M_VfxOverrides.bOverrideShellSpecificVfxOverwrites;
	Flags.bOverrideBounceEffect = M_VfxOverrides.bOverrideBounceEffect;
	Flags.bOverrideLaunchScale = M_VfxOverrides.bOverrideLaunchScale;
	Flags.bOverrideImpactScale = M_VfxOverrides.bOverrideImpactScale;
	Flags.bOverrideBounceScale = M_VfxOverrides.bOverrideBounceScale;
	Flags.bOverrideImpactAttenuation = M_VfxOverrides.bOverrideImpactAttenuation;
	Flags.bOverrideImpactConcurrency = M_VfxOverrides.bOverrideImpactConcurrency;
	Flags.bOverrideLaunchAttenuation = M_VfxOverrides.bOverrideLaunchAttenuation;
	Flags.bOverrideLaunchConcurrency = M_VfxOverrides.bOverrideLaunchConcurrency;
	return Flags;
}

void UBehWeaponOverwriteVFX::ApplyVfxOverrides(UWeaponState* WeaponState)
{
	if (not GetHasAnyVfxOverride())
	{
		return;
	}

	const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;
	if (M_PreviousVfxPerWeapon.Contains(WeaponPtr))
	{
		return;
	}

	FWeaponVFX* WeaponVfx = WeaponState->GetWeaponVfx();
	if (WeaponVfx == nullptr)
	{
		return;
	}

	FBehWeaponVfxOverrideRecord NewRecord;
	NewRecord.PreviousWeaponVfx = *WeaponVfx;
	NewRecord.OverrideFlags = BuildOverrideFlags();
	M_PreviousVfxPerWeapon.Add(WeaponPtr, NewRecord);
	ApplyOverridesToVfx(*WeaponVfx, M_VfxOverrides);
}

void UBehWeaponOverwriteVFX::RestoreVfxOverrides(UWeaponState* WeaponState)
{
	const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;
	const FBehWeaponVfxOverrideRecord* const CachedRecord = TryGetCachedVfxRecord(WeaponPtr);
	if (CachedRecord == nullptr)
	{
		return;
	}

	FWeaponVFX* WeaponVfx = WeaponState->GetWeaponVfx();
	if (WeaponVfx == nullptr)
	{
		return;
	}

	RestoreOverridesFromRecord(*WeaponVfx, *CachedRecord);
	M_PreviousVfxPerWeapon.Remove(WeaponPtr);
}

void UBehWeaponOverwriteVFX::ApplyOverridesToVfx(FWeaponVFX& WeaponVfx,
                                                 const FBehWeaponVfxOverrideSettings& Overrides) const
{
	if (Overrides.bOverrideLaunchSound)
	{
		WeaponVfx.LaunchSound = Overrides.LaunchSound;
	}

	if (Overrides.bOverrideBounceSound)
	{
		WeaponVfx.BounceSound = Overrides.BounceSound;
	}

	if (Overrides.bOverrideLaunchEffectSettings)
	{
		WeaponVfx.LaunchEffectSettings = Overrides.LaunchEffectSettings;
	}

	if (Overrides.bOverrideSurfaceImpactEffects)
	{
		WeaponVfx.SurfaceImpactEffects = Overrides.SurfaceImpactEffects;
	}

	if (Overrides.bOverrideShellSpecificVfxOverwrites)
	{
		WeaponVfx.ShellSpecificVfxOverwrites = Overrides.ShellSpecificVfxOverwrites;
	}

	if (Overrides.bOverrideBounceEffect)
	{
		WeaponVfx.BounceEffect = Overrides.BounceEffect;
	}

	if (Overrides.bOverrideLaunchScale)
	{
		WeaponVfx.LaunchScale = Overrides.LaunchScale;
	}

	if (Overrides.bOverrideImpactScale)
	{
		WeaponVfx.ImpactScale = Overrides.ImpactScale;
	}

	if (Overrides.bOverrideBounceScale)
	{
		WeaponVfx.BounceScale = Overrides.BounceScale;
	}

	if (Overrides.bOverrideImpactAttenuation)
	{
		WeaponVfx.ImpactAttenuation = Overrides.ImpactAttenuation;
	}

	if (Overrides.bOverrideImpactConcurrency)
	{
		WeaponVfx.ImpactConcurrency = Overrides.ImpactConcurrency;
	}

	if (Overrides.bOverrideLaunchAttenuation)
	{
		WeaponVfx.LaunchAttenuation = Overrides.LaunchAttenuation;
	}

	if (Overrides.bOverrideLaunchConcurrency)
	{
		WeaponVfx.LaunchConcurrency = Overrides.LaunchConcurrency;
	}
}

void UBehWeaponOverwriteVFX::RestoreOverridesFromRecord(FWeaponVFX& WeaponVfx,
                                                        const FBehWeaponVfxOverrideRecord& Record) const
{
	const FBehWeaponVfxOverrideFlags& Flags = Record.OverrideFlags;
	const FWeaponVFX& PreviousWeaponVfx = Record.PreviousWeaponVfx;

	if (Flags.bOverrideLaunchSound)
	{
		WeaponVfx.LaunchSound = PreviousWeaponVfx.LaunchSound;
	}

	if (Flags.bOverrideBounceSound)
	{
		WeaponVfx.BounceSound = PreviousWeaponVfx.BounceSound;
	}

	if (Flags.bOverrideLaunchEffectSettings)
	{
		WeaponVfx.LaunchEffectSettings = PreviousWeaponVfx.LaunchEffectSettings;
	}

	if (Flags.bOverrideSurfaceImpactEffects)
	{
		WeaponVfx.SurfaceImpactEffects = PreviousWeaponVfx.SurfaceImpactEffects;
	}

	if (Flags.bOverrideShellSpecificVfxOverwrites)
	{
		WeaponVfx.ShellSpecificVfxOverwrites = PreviousWeaponVfx.ShellSpecificVfxOverwrites;
	}

	if (Flags.bOverrideBounceEffect)
	{
		WeaponVfx.BounceEffect = PreviousWeaponVfx.BounceEffect;
	}

	if (Flags.bOverrideLaunchScale)
	{
		WeaponVfx.LaunchScale = PreviousWeaponVfx.LaunchScale;
	}

	if (Flags.bOverrideImpactScale)
	{
		WeaponVfx.ImpactScale = PreviousWeaponVfx.ImpactScale;
	}

	if (Flags.bOverrideBounceScale)
	{
		WeaponVfx.BounceScale = PreviousWeaponVfx.BounceScale;
	}

	if (Flags.bOverrideImpactAttenuation)
	{
		WeaponVfx.ImpactAttenuation = PreviousWeaponVfx.ImpactAttenuation;
	}

	if (Flags.bOverrideImpactConcurrency)
	{
		WeaponVfx.ImpactConcurrency = PreviousWeaponVfx.ImpactConcurrency;
	}

	if (Flags.bOverrideLaunchAttenuation)
	{
		WeaponVfx.LaunchAttenuation = PreviousWeaponVfx.LaunchAttenuation;
	}

	if (Flags.bOverrideLaunchConcurrency)
	{
		WeaponVfx.LaunchConcurrency = PreviousWeaponVfx.LaunchConcurrency;
	}
}

const FBehWeaponVfxOverrideRecord* UBehWeaponOverwriteVFX::TryGetCachedVfxRecord(
	const TWeakObjectPtr<UWeaponState>& WeaponPtr) const
{
	if (not WeaponPtr.IsValid())
	{
		return nullptr;
	}

	return M_PreviousVfxPerWeapon.Find(WeaponPtr);
}
