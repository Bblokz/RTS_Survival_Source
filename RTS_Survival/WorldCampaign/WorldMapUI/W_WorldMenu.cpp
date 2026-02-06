// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_WorldMenu.h"

#include "ArmyLayout/ArmyLayoutMenu/W_ArmyMenuLayout.h"
#include "PerkSystem/PerkMenu/W_WorldPerkMenu.h"
#include "RTS_Survival/GameUI/Archive/W_Archive/W_Archive.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

void UW_WorldMenu::UpdateMenuForNewFocus(const EWorldUIFocusState NewFocus)
{
	if (NewFocus == M_MenuFocusState)
	{
		return;
	}
	M_MenuFocusState = NewFocus;
	switch (NewFocus)
	{
	case EWorldUIFocusState::None:
		RTSFunctionLibrary::ReportError("Set None focus for world UI focus state!");
		break;
	case EWorldUIFocusState::CommandPerks:
		UpdateSwitchers(MenuSettings.PerkArmyWorldFullUI_Index, MenuSettings.PerkMenuIndex);
		break;
	case EWorldUIFocusState::ArmyLayout:
		UpdateSwitchers(MenuSettings.PerkArmyWorldFullUI_Index, MenuSettings.ArmyLayoutMenuIndex);
		break;
	case EWorldUIFocusState::World:
		UpdateSwitchers(MenuSettings.PerkArmyWorldFullUI_Index, 0);
		SetMenuSwitcherVisibility(false);
		break;
	case EWorldUIFocusState::Archive:
		UpdateSwitchers(MenuSettings.ArchiveFullUI_Index, 0);
		break;
	case EWorldUIFocusState::TechTree:
		UpdateSwitchers(MenuSettings.TechTreeFullUI_Index, 0);
		break;
	}
}

void UW_WorldMenu::OnPlayerExitsArchive()
{
	UpdateMenuForNewFocus(EWorldUIFocusState::World);
}

void UW_WorldMenu::InitWorldMenu(AWorldPlayerController* WorldPlayerController)
{
	M_PlayerController = WorldPlayerController;
	(void)GetIsValidPlayerController();
	InitMenu_InitArchive();
	InitMenu_InitHeader();
	InitMenu_InitPerkUI();
}

void UW_WorldMenu::UpdateSwitchers(const int32 FullUIIndex, const int32 MenuIndex) const
{
	if (FullUISwitcher)
	{
		FullUISwitcher->SetActiveWidgetIndex(FullUIIndex);
	}
	if (MenuSwitcher)
	{
		MenuSwitcher->SetActiveWidgetIndex(MenuIndex);
		SetMenuSwitcherVisibility(true);
	}
}

void UW_WorldMenu::SetMenuSwitcherVisibility(const bool bVisible) const
{
	const ESlateVisibility NewVis = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden;

	if (MenuSwitcher)
	{
		MenuSwitcher->SetVisibility(NewVis);
	}
}

bool UW_WorldMenu::GetIsValidPlayerController() const
{
	if(not M_PlayerController.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_PlayerController",
			"UW_WorldMenu::GetIsValidPlayerController",
			this
		);
		return false;
	}
	return true;	
}

void UW_WorldMenu::InitMenu_InitArchive()
{
	if(not ArchiveMenu)
	{
		return;
	}
	ArchiveMenu->SetWorldMenuUIReference(this);
}

void UW_WorldMenu::InitMenu_InitHeader()
{
	if(not WorldUIHeader)
	{
		return;
	}
	WorldUIHeader->InitWorldUIHeader(this);
}

void UW_WorldMenu::InitMenu_InitPerkUI()
{
	if(not PerkMenu)
	{
		return;
	}
	PerkMenu->InitPerkMenu(this);
}

void UW_WorldMenu::InitMenu_InitArmyLayout()
{
	if(not ArmyLayoutMenu)
	{
		return;
	}
	ArmyLayoutMenu->InitArmyLayoutMenu(this);
}


