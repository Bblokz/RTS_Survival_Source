// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_WorldMenu.h"

#include "ArmyLayout/ArmyLayoutMenu/W_ArmyMenuLayout.h"
#include "MapObjects/W_EnemyMapItem/W_EnemyOrMissionMapItem.h"
#include "MapObjects/W_EnemyMapItem/StrengthEstimation/W_StrengthEstimation.h"
#include "MapObjects/W_MissionReward/W_RewardCardsViewer/W_RewardCardsViewer.h"
#include "PerkSystem/PerkMenu/W_WorldPerkMenu.h"
#include "RTS_Survival/CardSystem/CardUI/CardMenu/W_CardMenu.h"
#include "RTS_Survival/CardSystem/DeckBuilder/W_DeckBuilderMenu.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
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
		ShowCardMenu();
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
	InitMenu_InitDeckBuilder();
	UpdateMenuForNewFocus(EWorldUIFocusState::World);
}

void UW_WorldMenu::ShowMissionMapItemDesc(AWorldMapObject* InitiatingWorldObject,
                                          const FEnemyOrMissionMapItemUIData& UIData,
                                          const FPrimaryReward& PrimaryReward,
                                          const FSecondaryReward& SecondaryReward)
{
	if (not MissionMapItemDesc)
	{
		return;
	}

	MissionMapItemDesc->SetupEnemyWidget(InitiatingWorldObject, UIData, PrimaryReward, SecondaryReward);
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

void UW_WorldMenu::UpdateTurnCounter(const int32 CurrentTurn) const
{
	if (not GetIsValidWorldUIHeader())
	{
		return;
	}

	WorldUIHeader->UpdateTurnCounter(CurrentTurn);
}

void UW_WorldMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (not MissionMapItemDesc)
	{
		return;
	}

	MissionMapItemDesc->InitAuxiliaryWidgets(RewardCardsViewer, StrengthEstimation);
	MissionMapItemDesc->OnLaunchRequested().AddUObject(this, &UW_WorldMenu::HandleMissionMapItemLaunchRequested);
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
	const ESlateVisibility StrengthEstimationVisibility =
		bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	if (RewardCardsViewer)
	{
		RewardCardsViewer->SetVisibility(NewVis);
	}
	if (StrengthEstimation)
	{
		StrengthEstimation->SetVisibility(StrengthEstimationVisibility);
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
	if (not GetIsValidWorldUIHeader())
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

void UW_WorldMenu::InitMenu_InitDeckBuilder()
{
	if (not bUseDeckBuilderMenu || not FullUISwitcher)
	{
		return;
	}
	const TSubclassOf<UW_DeckBuilderMenu> MenuClass = DeckBuilderMenuClass
		                                                 ? DeckBuilderMenuClass
		                                                 : TSubclassOf<UW_DeckBuilderMenu>(
			                                                 UW_DeckBuilderMenu::StaticClass());
	M_DeckBuilderMenu = CreateWidget<UW_DeckBuilderMenu>(this, MenuClass);
	if (not IsValid(M_DeckBuilderMenu))
	{
		RTSFunctionLibrary::ReportError("Failed to create the deck builder menu; the old card menu is shown instead.");
		return;
	}
	M_DeckBuilderMenu->InitDeckBuilder(this);
	FullUISwitcher->AddChild(M_DeckBuilderMenu);
	M_DeckBuilderSwitcherIndex = FullUISwitcher->GetChildIndex(M_DeckBuilderMenu);
}

void UW_WorldMenu::ShowCardMenu()
{
	const UWorldStateAndSaveManager* const SaveManager = M_PlayerController.IsValid()
		                                                     ? M_PlayerController->GetWorldStateAndSaveManager()
		                                                     : nullptr;
	if (not IsValid(M_DeckBuilderMenu) || M_DeckBuilderSwitcherIndex == INDEX_NONE || not IsValid(SaveManager))
	{
		UpdateUISwitch(MenuSettings.CardMenuFullUI_Index);
		return;
	}
	// Filled from the campaign's cached cards on every open, so the menu never shows a deck it kept itself.
	M_DeckBuilderMenu->SetupFromCardData(SaveManager->GetCachedPlayerProfileSaveData().PlayerData.CardData);
	UpdateUISwitch(M_DeckBuilderSwitcherIndex);
}

void UW_WorldMenu::HandleDeckBuilderSaved(const FPlayerCardSaveData& CardData)
{
	if (not GetIsValidPlayerController())
	{
		return;
	}
	UWorldStateAndSaveManager* const SaveManager = M_PlayerController->GetWorldStateAndSaveManager();
	if (not IsValid(SaveManager))
	{
		RTSFunctionLibrary::ReportError("The world menu found no save manager; the deck was not saved.");
		return;
	}
	SaveManager->CachePlayerCardData(CardData);
	// Saved right away: launching a mission opens its level without saving the campaign first.
	SaveManager->SaveCampaignState();
	UpdateMenuForNewFocus(EWorldUIFocusState::World);
}

UW_DeckBuilderMenu* UW_WorldMenu::GetDeckBuilderMenu() const
{
	return M_DeckBuilderMenu;
}

void UW_WorldMenu::HandleMissionMapItemLaunchRequested(AWorldMapObject* OperationWorldObject)
{
	if (not GetIsValidPlayerController())
	{
		return;
	}

	M_PlayerController->LaunchOperationForWorldObject(OperationWorldObject);
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

bool UW_WorldMenu::GetIsValidWorldUIHeader() const
{
	if (IsValid(WorldUIHeader))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("WorldUIHeader"),
		TEXT("UW_WorldMenu::GetIsValidWorldUIHeader"),
		this
	);
	return false;
}
