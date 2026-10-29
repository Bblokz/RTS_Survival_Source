// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Units/Variants/UnitVariantTypes.h"
#include "UnitVariantSelectionStyle.h"

#include "W_UnitVariantCard.generated.h"

class UBorder;
class UButton;
class UImage;
class UOverlay;
class UTextBlock;
class UW_UnitVariantSelectionMenu;
class UWrapBox;
struct FUnitVariantAbilityCard;

/**
 * @brief One variant (Base, Variant A or Variant B) of the unit selected in the unit variant selection screen;
 * created by UW_UnitVariantSelectionMenu, builds its own widget tree and tells the menu when it is chosen.
 */
UCLASS()
class RTS_SURVIVAL_API UW_UnitVariantCard : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the card; call once right after creating it.
	 * @param Menu Receives the click.
	 * @param Variant Which variant of the unit this card stands for.
	 * @param Style Brushes, fonts and colours shared by the whole screen.
	 * @param IconBrush The variant's picture; its resource must be held by an asset the menu keeps loaded.
	 * @param VariantName Plain text name of the variant.
	 * @param Description What the variant changes.
	 */
	void SetupCard(UW_UnitVariantSelectionMenu* Menu, const EUnitVariant Variant, const FUnitVariantSelectionStyle& Style,
	               const FSlateBrush& IconBrush, const FText& VariantName, const FText& Description);

	void SetIsSelected(const bool bIsSelected);

	/** Arrow key / d-pad focus: the card shows its hovered look until the focus moves on. */
	void SetIsKeyboardFocused(const bool bIsKeyboardFocused);

	/** Fills the row of small ability icons under the description; called once the variant's class has loaded. */
	void SetAbilityIcons(const TArray<FUnitVariantAbilityCard>& AbilityCards);

	EUnitVariant GetVariant() const { return M_Variant; }

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
	TObjectPtr<UTextBlock> M_LabelText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_DescriptionText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_EquippedText;

	UPROPERTY()
	TObjectPtr<UImage> M_SelectedMarker;

	// Wraps when a variant has more abilities than fit next to each other in the text column.
	UPROPERTY()
	TObjectPtr<UWrapBox> M_AbilityIconRow;

	// Copied from the menu so selection can swap the button style, the icon well and the text colours.
	UPROPERTY()
	FUnitVariantSelectionStyle M_Style;

	EUnitVariant M_Variant = EUnitVariant::Base;
	bool bM_IsSelected = false;
	bool bM_IsKeyboardFocused = false;

	void BuildWidgetTree();
	void BuildIconWell(UOverlay* CardOverlay);
	void BuildTextColumn(UOverlay* CardOverlay);
	void BuildEquippedBadge(UOverlay* CardOverlay);
	void ApplySelectionLook();

	UFUNCTION()
	void OnCardClicked();

	UFUNCTION()
	void OnCardHovered();

	UFUNCTION()
	void OnCardUnhovered();

	bool GetIsValidMenu() const;
	bool GetIsValidButton() const;
	bool GetIsValidAbilityIconRow() const;
};
