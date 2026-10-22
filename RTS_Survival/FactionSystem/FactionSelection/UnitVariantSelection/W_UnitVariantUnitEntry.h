// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnitVariantSelectionStyle.h"

#include "W_UnitVariantUnitEntry.generated.h"

class UBorder;
class UButton;
class UImage;
class UOverlay;
class UTextBlock;
class UW_UnitVariantSelectionMenu;

/**
 * @brief One unit in the left list of the unit variant selection screen; created by UW_UnitVariantSelectionMenu,
 * builds its own widget tree and tells the menu when it is clicked.
 */
UCLASS()
class RTS_SURVIVAL_API UW_UnitVariantUnitEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the entry; call once right after creating it.
	 * @param Menu Receives the click.
	 * @param EntryIndex Index of the unit in the menu's unit list.
	 * @param Style Brushes, fonts and colours shared by the whole screen.
	 * @param UnitIcon The unit's picture; a brush without a resource leaves the well empty.
	 * @param UnitName Plain text name of the unit.
	 * @param VariantCount How many variants the unit has, Base included.
	 */
	void SetupEntry(UW_UnitVariantSelectionMenu* Menu, const int32 EntryIndex, const FUnitVariantSelectionStyle& Style,
	                const FSlateBrush& UnitIcon, const FText& UnitName, const int32 VariantCount);

	void SetIsSelected(const bool bIsSelected);

	/** Shows which variant is currently chosen for the unit next to its variant count. */
	void SetChosenVariantName(const FText& ChosenVariantName);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_UnitVariantSelectionMenu> M_Menu;

	UPROPERTY()
	TObjectPtr<UButton> M_Button;

	UPROPERTY()
	TObjectPtr<UBorder> M_IconWell;

	UPROPERTY()
	TObjectPtr<UImage> M_Icon;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_DetailText;

	// Copied from the menu so selection can swap the button style, the icon well and the text colours.
	UPROPERTY()
	FUnitVariantSelectionStyle M_Style;

	int32 M_EntryIndex = INDEX_NONE;
	int32 M_VariantCount = 1;
	bool bM_IsSelected = false;

	void BuildWidgetTree();
	void BuildIconWell(UOverlay* EntryOverlay);
	void BuildTexts(UOverlay* EntryOverlay);
	void ApplySelectionLook();

	UFUNCTION()
	void OnEntryClicked();

	bool GetIsValidMenu() const;
	bool GetIsValidButton() const;
};
