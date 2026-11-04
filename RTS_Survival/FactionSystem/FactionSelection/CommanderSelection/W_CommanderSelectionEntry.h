// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommanderSelectionData.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionStyle.h"

#include "W_CommanderSelectionEntry.generated.h"

class UBorder;
class UButton;
class UImage;
class UOverlay;
class UTextBlock;
class UTexture2D;
class UW_CommanderSelectionMenu;

/**
 * @brief One commander in the left list of the commander selection screen; created by UW_CommanderSelectionMenu,
 * builds its own widget tree and tells the menu when it is clicked.
 */
UCLASS()
class RTS_SURVIVAL_API UW_CommanderSelectionEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the entry; call once right after creating it. The portrait follows with SetPortrait once loaded.
	 * @param Menu Receives the click.
	 * @param EntryIndex Index of the commander in the menu's list.
	 * @param Style Brushes, fonts and colours of the selection screens.
	 * @param CommanderEntry Supplies the title.
	 */
	void SetupEntry(UW_CommanderSelectionMenu* Menu, const int32 EntryIndex, const FUnitVariantSelectionStyle& Style,
	                const FCommanderSelectionEntryData& CommanderEntry);

	/** Shows the head of the portrait in the icon well; null leaves the well empty. */
	void SetPortrait(UTexture2D* Portrait);

	void SetIsSelected(const bool bIsSelected);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_CommanderSelectionMenu> M_Menu;

	UPROPERTY()
	TObjectPtr<UButton> M_Button;

	UPROPERTY()
	TObjectPtr<UBorder> M_PortraitWell;

	UPROPERTY()
	TObjectPtr<UImage> M_PortraitImage;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TitleText;

	// Copied from the menu so selection can swap the button style, the well and the text colours.
	UPROPERTY()
	FUnitVariantSelectionStyle M_Style;

	int32 M_EntryIndex = INDEX_NONE;
	bool bM_IsSelected = false;

	void BuildWidgetTree();
	void BuildPortraitWell(UOverlay* EntryOverlay);
	void BuildTitle(UOverlay* EntryOverlay);
	void ApplySelectionLook();

	UFUNCTION()
	void OnEntryClicked();

	bool GetIsValidMenu() const;
	bool GetIsValidButton() const;
};
