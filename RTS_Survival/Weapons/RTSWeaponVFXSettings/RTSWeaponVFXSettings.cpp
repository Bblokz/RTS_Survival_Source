// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSWeaponVFXSettings.h"

#include "NiagaraSystem.h"

URTSWeaponVFXSettings::URTSWeaponVFXSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("RTS Weapon VFX");
	TraceImpactNiagaraSystem = FSoftObjectPath(
		TEXT("/Game/RTS_Survival/ImpactSfxVfx/TraceImpacts/NS_TraceImpacts.NS_TraceImpacts"));
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
