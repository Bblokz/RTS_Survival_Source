// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSWeaponVFXSettings.h"

#include "NiagaraSystem.h"

URTSWeaponVFXSettings::URTSWeaponVFXSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("RTS Weapon VFX");
	TraceImpactNiagaraSystem = FSoftObjectPath(
		TEXT("/Game/RTS_Survival/ImpactSfxVfx/TraceImpacts/NS_TraceImpacts.NS_TraceImpacts"));
	SmallArmsMuzzleNiagaraSystem = FSoftObjectPath(
		TEXT("/Game/RTS_Survival/ImpactSfxVfx/MuzzleVfx/NS_SmallArmsMuzzle.NS_SmallArmsMuzzle"));
	// The launch effects the infantry weapons were given before the batcher; see BPF_ImpactHelpers.
	SmallArmsLaunchEffectsToBatch = {
		TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT(
			"/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/VFX/NS_MuzzleFlash_SmallArmsRandomBurst.NS_MuzzleFlash_SmallArmsRandomBurst"))),
		TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT(
			"/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/VFX/NS_Single_MuzzleFlash.NS_Single_MuzzleFlash"))),
		TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT(
			"/Game/RTS_Survival/Blueprints/Turrets/German/Light/PumaTurrets/50mmPuma/VFX/NS_PumaMG_LaunchEffect.NS_PumaMG_LaunchEffect")))
	};
	for (const ESmallArmsMuzzleClass MuzzleClass : {
		     ESmallArmsMuzzleClass::Pistol, ESmallArmsMuzzleClass::SubmachineGun, ESmallArmsMuzzleClass::AssaultRifle,
		     ESmallArmsMuzzleClass::Rifle, ESmallArmsMuzzleClass::MachineGun, ESmallArmsMuzzleClass::AntiTankRifle
	     })
	{
		SmallArmsMuzzleProfiles.Add(MuzzleClass, SmallArmsMuzzleClass::GetDefaultProfile(MuzzleClass));
	}
}

FSmallArmsMuzzleProfile URTSWeaponVFXSettings::GetSmallArmsMuzzleProfile(const ESmallArmsMuzzleClass MuzzleClass) const
{
	if (const FSmallArmsMuzzleProfile* const Found = SmallArmsMuzzleProfiles.Find(MuzzleClass))
	{
		return *Found;
	}
	return SmallArmsMuzzleClass::GetDefaultProfile(MuzzleClass);
}

bool URTSWeaponVFXSettings::GetIsSmallArmsLaunchEffectToBatch(const UNiagaraSystem* LaunchEffect) const
{
	if (LaunchEffect == nullptr)
	{
		return false;
	}
	const FSoftObjectPath LaunchEffectPath(LaunchEffect);
	for (const TSoftObjectPtr<UNiagaraSystem>& EffectToBatch : SmallArmsLaunchEffectsToBatch)
	{
		if (EffectToBatch.ToSoftObjectPath() == LaunchEffectPath)
		{
			return true;
		}
	}
	return false;
}

const URTSWeaponVFXSettings* URTSWeaponVFXSettings::Get()
{
	return GetDefault<URTSWeaponVFXSettings>();
}

FLinearColor URTSWeaponVFXSettings::ResolveColorForShell(const EWeaponShellType ShellType) const
{
	if (const FLinearColor* const Found = ShellTypeColors.Find(ShellType))
	{
		return *Found;
	}
	return FLinearColor::Transparent;
}
