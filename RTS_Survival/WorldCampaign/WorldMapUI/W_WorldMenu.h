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


class UW_CardMenu;
class AWorldPlayerController;

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

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FWorldMenuSettings MenuSettings;

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

private:
	UPROPERTY()
	EWorldUIFocusState M_MenuFocusState = EWorldUIFocusState::None;

	void UpdateUISwitch(const int32 FullUIIndex) const;
	void SetPerkMenuVisibility(const bool bVisible) const;
	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_PlayerController;
	bool GetIsValidPlayerController() const;

	void InitMenu_InitArchive();
	void InitMenu_InitHeader();
	void InitMenu_InitPerkUI();
	void InitMenu_InitCardMenu();

	UPROPERTY()
	TWeakObjectPtr<UWorldProfileAndUIManager> M_PlayerProfileAndUIManager;
	bool GetIsValidProfileAndUIManager() const;
};
