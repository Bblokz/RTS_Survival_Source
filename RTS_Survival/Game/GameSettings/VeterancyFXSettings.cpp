// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/Game/GameSettings/VeterancyFXSettings.h"

UVeterancyFXSettings::UVeterancyFXSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Veterancy FX");
}

const UVeterancyFXSettings* UVeterancyFXSettings::Get()
{
	return GetDefault<UVeterancyFXSettings>();
}
