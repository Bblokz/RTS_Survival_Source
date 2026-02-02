// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignSettings.h"

UWorldCampaignSettings::UWorldCampaignSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("World Campaign");
}

const UWorldCampaignSettings* UWorldCampaignSettings::Get()
{
	return GetDefault<UWorldCampaignSettings>();
}
