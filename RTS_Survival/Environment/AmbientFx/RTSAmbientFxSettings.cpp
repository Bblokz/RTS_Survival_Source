// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSAmbientFxSettings.h"

#include "NiagaraSystem.h"

URTSAmbientFxSettings::URTSAmbientFxSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("RTS Ambient Effects");
	DustDevilNiagaraSystem = FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Blueprints/Environment/AmbientFx/NS_AmbientDustDevils.NS_AmbientDustDevils"));
	SandMaskFolder.Path = TEXT("/Game/RTS_Survival/Blueprints/Environment/AmbientFx/SandMasks");
}

const URTSAmbientFxSettings* URTSAmbientFxSettings::Get()
{
	return GetDefault<URTSAmbientFxSettings>();
}

FSoftObjectPath URTSAmbientFxSettings::GetSandMaskPath(const FString& MapName) const
{
	const FString AssetName = TEXT("SandMask_") + MapName;
	return FSoftObjectPath(SandMaskFolder.Path / AssetName + TEXT(".") + AssetName);
}
