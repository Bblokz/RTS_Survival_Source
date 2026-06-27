// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/SaveAndState/SaveData/FWorldCampaignState.h"

bool FWorldCampaignState::GetIsValidForRestore() const
{
	return Anchors.Num() > 0;
}
