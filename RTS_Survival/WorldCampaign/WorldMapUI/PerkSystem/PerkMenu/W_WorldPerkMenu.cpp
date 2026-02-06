// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_WorldPerkMenu.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"

void UW_WorldPerkMenu::InitPerkMenu(UW_WorldMenu* WorldMenu)
{
	M_WorldMenu = WorldMenu;	
}

bool UW_WorldPerkMenu::GetIsValidWorldMenu() const
{
	if(not M_WorldMenu.IsValid())
	{
		RTSFunctionLibrary::ReportError("WorldMenu is not valid in WorldPerkMenu, cannot proceed.");
		return false;
	}
	return true;
}
