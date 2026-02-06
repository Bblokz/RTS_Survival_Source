// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Header/W_WorldUIHeader.h"
#include "WorldUIFocusState/WorldUIFocusState.h"
#include "W_WorldMenu.generated.h"


class AWorldPlayerController;

USTRUCT(BlueprintType)
struct FWorldMenuSettings
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	int32 PerkMenuIndex = 0;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	int32 ArmyLayoutMenuIndex = 1;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	int32 PerkArmyWorldFullUI_Index = 0;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	int32 ArchiveFullUI_Index = 1;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	int32 TechTreeFullUI_Index = 2;
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

	void InitWorldMenu(AWorldPlayerController* WorldPlayerController);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FWorldMenuSettings MenuSettings;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_WorldUIHeader> WorldUIHeader;
	// Used to swap the full canvas panel with the archive UI.
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> FullUISwitcher;

	// To switch between the CommandPerks and the ArmyLayout menus.
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> MenuSwitcher;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_WorldPerkMenu> PerkMenu;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_ArmyLayoutMenu> ArmyLayoutMenu;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_Archive> ArchiveMenu;

private:
	UPROPERTY()
	EWorldUIFocusState M_MenuFocusState = EWorldUIFocusState::None;

	void UpdateSwitchers(const int32 FullUIIndex, const int32 MenuIndex) const;
	void SetMenuSwitcherVisibility(const bool bVisible) const;
	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_PlayerController;
	bool GetIsValidPlayerController() const;

	void InitMenu_InitArchive();
	void InitMenu_InitHeader();
	void InitMenu_InitPerkUI();
	void InitMenu_InitArmyLayout();
};
