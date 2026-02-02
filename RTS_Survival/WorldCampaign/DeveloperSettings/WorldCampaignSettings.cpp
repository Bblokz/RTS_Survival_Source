// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignSettings.h"

UWorldCampaignSettings::UWorldCampaignSettings()
{
}

const UWorldCampaignSettings* UWorldCampaignSettings::Get()
{
	return GetDefault<UWorldCampaignSettings>();
}
