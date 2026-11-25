// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Header/W_WorldUIHeader.h"
#include "RTS_Survival/WorldCampaign/PlayerProfile/FPlayerProfileSaveData.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerProfileAndUIManager/WorldProfileAndUIManager.h"
#include "WorldUIFocusState/WorldUIFocusState.h"
#include "W_WorldMenu.generated.h"


class UW_StrengthEstimation;
class UW_EnemyOrMissionMapItem;
class UW_RewardCardsViewer;
class UW_CardMenu;
class UW_DeckBuilderMenu;
class UW_WorldCampaignHUD;
class UW_WorldTechWheel;
class AWorldPlayerController;
struct FPlayerCardSaveData;
struct FEnemyOrMissionMapItemUIData;
struct FPrimaryReward;
struct FSecondaryReward;

USTRUCT(BlueprintType)
struct FWorldMenuSettings
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	int32 PerkWorldFullUI_Index = 0;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	int32 ArchiveFullUI_Index = 1;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	int32 CardMenuFullUI_Index = 2;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	int32 TechTreeFullUI_Index = 3;
};

class UW_Archive;
class UW_ArmyLayoutMenu;
class UW_WorldPerkMenu;
class AWorldMapObject;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	void UpdateMenuForNewFocus(EWorldUIFocusState NewFocus);
	/** @return what part of the UI is currently visible for the player */
	EWorldUIFocusState GetCurrentMenuFocusState() const { return M_MenuFocusState; }
	void OnPlayerExitsArchive();

	void InitWorldMenu(AWorldPlayerController* WorldPlayerController, UWorldProfileAndUIManager* PlayerProfileUIManager);
	void SetupUIForPlayerProfile(const FPlayerData& PlayerProfileSaveData);
	void UpdateTurnCounter(int32 CurrentTurn) const;
	/**
	 * @brief Opens the compact map-item panel from controller-selected world actors.
	 * @param InitiatingWorldObject World map actor that owns the displayed operation.
	 * @param UIData Text and strength data authored on the clicked world map object.
	 * @param PrimaryReward Rewards shown immediately and used by the card preview.
	 * @param SecondaryReward Optional follow-up reward data shown in the reward panel.
	 */
	void ShowMissionMapItemDesc(AWorldMapObject* InitiatingWorldObject,
	                            const FEnemyOrMissionMapItemUIData& UIData,
	                            const FPrimaryReward& PrimaryReward,
	                            const FSecondaryReward& SecondaryReward);
	void CollapseMissionMapItemDesc() const;

	/** The deck builder closed: stores its deck in the campaign save and returns to the map. */
	void HandleDeckBuilderSaved(const FPlayerCardSaveData& CardData);

	/** @return Null while the old Blueprint card menu is in use. */
	UW_DeckBuilderMenu* GetDeckBuilderMenu() const;

	/** @return The campaign widgets drawn over the map; null when bUseCampaignHUD is off. */
	UW_WorldCampaignHUD* GetCampaignHUD() const { return M_CampaignHUD; }

	/** @return The tech wheel page; null when bUseCampaignHUD is off. */
	UW_WorldTechWheel* GetTechWheel() const { return M_TechWheel; }

protected:
	// The card menu the Army Layout button opens: the deck builder, or (off) the old Blueprint card menu.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Menu")
	bool bUseDeckBuilderMenu = true;

	// Left empty the C++ deck builder is used as it is; pick a Blueprint child of it to override its art.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Menu")
	TSubclassOf<UW_DeckBuilderMenu> DeckBuilderMenuClass;

	// The C++ campaign HUD (top strip, map tags, commander panel, build cards, tech wheel page). When on, it replaces
	// the Blueprint header, which is collapsed; off leaves the Blueprint header and tech tree page as they were.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Campaign HUD")
	bool bUseCampaignHUD = true;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnMissionClicked(const FEnemyOrMissionMapItemUIData& InData);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FWorldMenuSettings MenuSettings;
	
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_WorldUIHeader> WorldUIHeader;
	// Used to swap the full canvas panel with the archive UI.
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> FullUISwitcher;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_WorldPerkMenu> PerkMenu;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_CardMenu> CardMenu;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_Archive> ArchiveMenu;
	
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_EnemyOrMissionMapItem> MissionMapItemDesc;
	
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_RewardCardsViewer> RewardCardsViewer;
	
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	 TObjectPtr<UW_StrengthEstimation> StrengthEstimation;

private:
	UPROPERTY()
	EWorldUIFocusState M_MenuFocusState = EWorldUIFocusState::None;

	void UpdateUISwitch(const int32 FullUIIndex) const;
	void SetPerkMenuVisibility(const bool bVisible) const;
	void SetMissionDescVisibility(const bool bVisible)const;
	void SetAuxiliaryMapItemsWidgetsVisibility(const bool bVisible)const;
	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_PlayerController;
	bool GetIsValidPlayerController() const;

	void InitMenu_InitArchive();
	void InitMenu_InitHeader();
	void InitMenu_InitPerkUI();
	void InitMenu_InitCardMenu();
	void InitMenu_InitDeckBuilder();
	void InitMenu_InitCampaignHUD();
	void HandleTechWheelCloseRequested();

	UPROPERTY()
	TObjectPtr<UW_WorldCampaignHUD> M_CampaignHUD;

	UPROPERTY()
	TObjectPtr<UW_WorldTechWheel> M_TechWheel;

	// Page of the tech wheel in FullUISwitcher; added at runtime like the deck builder.
	int32 M_TechWheelSwitcherIndex = INDEX_NONE;

	/** Shows the deck builder with the campaign's current cards, or the old card menu when it is not in use. */
	void ShowCardMenu();

	UPROPERTY()
	TObjectPtr<UW_DeckBuilderMenu> M_DeckBuilderMenu;

	// Page of the deck builder in FullUISwitcher; it is added at runtime, after the Blueprint's own pages.
	int32 M_DeckBuilderSwitcherIndex = INDEX_NONE;
	void HandleMissionMapItemLaunchRequested(AWorldMapObject* OperationWorldObject);
	bool GetIsValidWorldUIHeader() const;

	UPROPERTY()
	TWeakObjectPtr<UWorldProfileAndUIManager> M_PlayerProfileAndUIManager;
	bool GetIsValidProfileAndUIManager() const;
};
