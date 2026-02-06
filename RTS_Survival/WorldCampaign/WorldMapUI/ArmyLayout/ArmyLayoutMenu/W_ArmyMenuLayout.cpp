// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_ArmyMenuLayout.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"

void UW_ArmyLayoutMenu::InitArmyLayoutMenu(UW_WorldMenu* WorldMenu)
{
	M_WorldMenu = WorldMenu;
}

bool UW_ArmyLayoutMenu::GetIsValidWorldMenu() const
{
	if(not M_WorldMenu.IsValid())
	{
		RTSFunctionLibrary::ReportError("WorldMenu is not valid for ArmyLayoutMenu, please check the widget blueprint.");
		return false;
	}
	return true;
}
