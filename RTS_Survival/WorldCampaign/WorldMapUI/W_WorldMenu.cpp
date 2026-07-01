// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_WorldMenu.h"

#include "ArmyLayout/ArmyLayoutMenu/W_ArmyMenuLayout.h"
#include "MapObjects/W_EnemyMapItem/W_EnemyOrMissionMapItem.h"
#include "MapObjects/W_EnemyMapItem/StrengthEstimation/W_StrengthEstimation.h"
#include "MapObjects/W_MissionReward/W_RewardCardsViewer/W_RewardCardsViewer.h"
#include "PerkSystem/PerkMenu/W_WorldPerkMenu.h"
#include "RTS_Survival/CardSystem/CardUI/CardMenu/W_CardMenu.h"
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
	SetAuxiliaryMapItemsWidgetsVisibility(false);
	
	switch (NewFocus)
	{
	case EWorldUIFocusState::None:
		RTSFunctionLibrary::ReportError("Set None focus for world UI focus state!");
		break;
	case EWorldUIFocusState::CommandPerks:
		SetMissionDescVisibility(false);
		SetPerkMenuVisibility(true);
		UpdateUISwitch(MenuSettings.PerkWorldFullUI_Index);
		break;
	case EWorldUIFocusState::CardMenu:
		UpdateUISwitch(MenuSettings.CardMenuFullUI_Index);
		break;
	case EWorldUIFocusState::World:
		SetPerkMenuVisibility(false);
		SetMissionDescVisibility(false);
		UpdateUISwitch(MenuSettings.PerkWorldFullUI_Index);
		break;
	case EWorldUIFocusState::Archive:
		UpdateUISwitch(MenuSettings.ArchiveFullUI_Index);
		break;
	case EWorldUIFocusState::TechTree:
		UpdateUISwitch(MenuSettings.TechTreeFullUI_Index);
		break;
	}
}

void UW_WorldMenu::OnPlayerExitsArchive()
{
	UpdateMenuForNewFocus(EWorldUIFocusState::World);
}

void UW_WorldMenu::InitWorldMenu(AWorldPlayerController* WorldPlayerController,
                                 UWorldProfileAndUIManager* PlayerProfileUIManager)
{
	M_PlayerController = WorldPlayerController;
	(void)GetIsValidPlayerController();
	M_PlayerProfileAndUIManager = PlayerProfileUIManager;
	(void)GetIsValidProfileAndUIManager();
	InitMenu_InitArchive();
	InitMenu_InitHeader();
	InitMenu_InitPerkUI();
	InitMenu_InitCardMenu();
	UpdateMenuForNewFocus(EWorldUIFocusState::World);
}

void UW_WorldMenu::ShowMissionMapItemDesc(const FEnemyOrMissionMapItemUIData& UIData,
                                             const FPrimaryReward& PrimaryReward,
                                             const FSecondaryReward& SecondaryReward)
{
	if (not MissionMapItemDesc)
	{
		return;
	}

	MissionMapItemDesc->SetupEnemyWidget(UIData, PrimaryReward, SecondaryReward);
	SetMissionDescVisibility(true);
	BP_OnMissionClicked(UIData);
}

void UW_WorldMenu::CollapseMissionMapItemDesc() const
{
	SetMissionDescVisibility(false);
	SetAuxiliaryMapItemsWidgetsVisibility(false);
}

void UW_WorldMenu::SetupUIForPlayerProfile(const FPlayerData& PlayerProfileSaveData)
{
	if (not ArchiveMenu || not PerkMenu || not CardMenu)
	{
		RTSFunctionLibrary::ReportError(
			"One of the world menu subwidgets is not valid, cannot setup world menu with player profile data.");
		return;
	}
	PerkMenu->SetupUIWithPlayerProfile(PlayerProfileSaveData.PerkData);
	CardMenu->SetupCardMenuFromProfile(PlayerProfileSaveData.CardData);
	ArchiveMenu->SetupArchiveWithPlayerProfileSaveData(PlayerProfileSaveData.ArchiveData);
}

void UW_WorldMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (not MissionMapItemDesc)
	{
		return;
	}

	MissionMapItemDesc->InitAuxiliaryWidgets(RewardCardsViewer, StrengthEstimation);
}

void UW_WorldMenu::UpdateUISwitch(const int32 FullUIIndex) const
{
	if (FullUISwitcher)
	{
		FullUISwitcher->SetActiveWidgetIndex(FullUIIndex);
	}
}

void UW_WorldMenu::SetPerkMenuVisibility(const bool bVisible) const
{
	const ESlateVisibility NewVis = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden;

	if (PerkMenu)
	{
		PerkMenu->SetVisibility(NewVis);
	}
}

void UW_WorldMenu::SetMissionDescVisibility(const bool bVisible) const
{
	if (not MissionMapItemDesc)
	{
		return;
	}
	const ESlateVisibility NewVis = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	MissionMapItemDesc->SetVisibility(NewVis);
	if (bVisible)
	{
		MissionMapItemDesc->ShowVisibleAnimation();
	}
}

void UW_WorldMenu::SetAuxiliaryMapItemsWidgetsVisibility(const bool bVisible) const
{
	const ESlateVisibility NewVis = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	if (RewardCardsViewer)
	{
		RewardCardsViewer->SetVisibility(NewVis);
	}
	if (StrengthEstimation)
	{
		StrengthEstimation->SetVisibility(NewVis);
	}
}

bool UW_WorldMenu::GetIsValidPlayerController() const
{
	if (not M_PlayerController.IsValid())
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
	if (not ArchiveMenu)
	{
		return;
	}
	ArchiveMenu->SetWorldMenuUIReference(this);
}

void UW_WorldMenu::InitMenu_InitHeader()
{
	if (not WorldUIHeader)
	{
		return;
	}
	WorldUIHeader->InitWorldUIHeader(this);
}

void UW_WorldMenu::InitMenu_InitPerkUI()
{
	if (not PerkMenu)
	{
		return;
	}
	PerkMenu->InitPerkMenu(this);
}

void UW_WorldMenu::InitMenu_InitCardMenu()
{
	if (not CardMenu)
	{
		return;
	}
	CardMenu->InitCardMenu(this);
}

bool UW_WorldMenu::GetIsValidProfileAndUIManager() const
{
	if (not M_PlayerProfileAndUIManager.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_ProfileAndUIManager",
			"UW_WorldMenu::GetIsValidProfileAndUIManager",
			this
		);
		return false;
	}
	return true;
}
