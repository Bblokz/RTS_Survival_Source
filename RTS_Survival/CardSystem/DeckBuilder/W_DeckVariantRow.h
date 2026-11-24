// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeckCardDragOperation.h"

#include "W_DeckVariantRow.generated.h"

class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UW_DeckBuilderMenu;
struct FDeckBuilderLook;

/**
 * @brief One choice in the deck builder's dock: Base or an owned variant of the unit the dock shows, in the hangar
 * variant screen's card art. Created by the deck builder menu, which it tells when it is clicked. Builds its own
 * widget tree.
 */
UCLASS()
class RTS_SURVIVAL_API UW_DeckVariantRow : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the row.
	 * @param Menu Told when the row is clicked.
	 * @param Look The menu's art.
	 * @param View Label, name, text and picture; View.Variant.Variant is Base for the base row.
	 * @param bIsEquipped Whether this is what the unit runs now.
	 */
	void SetupRow(UW_DeckBuilderMenu* Menu, const FDeckBuilderLook& Look, const FDeckVariantCardView& View,
	              const bool bIsEquipped);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildWidgetTree();

	UFUNCTION()
	void OnRowClicked();

	bool GetIsValidMenu() const;

	UPROPERTY()
	TWeakObjectPtr<UW_DeckBuilderMenu> M_Menu;

	FUnitVariantSelection M_Variant;

	UPROPERTY()
	TObjectPtr<UButton> M_Button;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_Canvas;

	UPROPERTY()
	TObjectPtr<UImage> M_Picture;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_LabelText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_DescriptionText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_EquippedText;

	UPROPERTY()
	TObjectPtr<UImage> M_EquippedMarker;
};
