// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeckCardDragOperation.h"

#include "W_DeckVariantCard.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UW_DeckBuilderMenu;
struct FDeckBuilderLook;

// Where a variant card is shown, which decides what a click or a drag on it does.
UENUM()
enum class EDeckVariantCardPlace : uint8
{
	// Loose in the collection: drag it onto its unit card, or right-click to attach it.
	Collection,
	// Tucked behind its unit card: right-click detaches it.
	Clipped,
	// The dock and the picture that follows the cursor during a drag: no input.
	Preview
};

/**
 * @brief The narrow variant card with its painted spine, created by the deck builder menu and by the unit cards a
 * variant is clipped onto. Builds its own widget tree; the menu's look supplies the art.
 */
UCLASS()
class RTS_SURVIVAL_API UW_DeckVariantCard : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the card.
	 * @param Menu Receives the hover, right-click and drop of this card.
	 * @param Look The menu's art.
	 * @param View Name, pictures and rarity of the variant.
	 * @param Place Where the card is shown.
	 */
	void SetupVariantCard(UW_DeckBuilderMenu* Menu, const FDeckBuilderLook& Look, const FDeckVariantCardView& View,
	                      const EDeckVariantCardPlace Place);

	const FUnitVariantSelection& GetVariant() const { return M_View.Variant; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
	                                  UDragDropOperation*& OutOperation) override;

private:
	void BuildWidgetTree();
	bool GetIsValidMenu() const;

	UPROPERTY()
	TWeakObjectPtr<UW_DeckBuilderMenu> M_Menu;

	FDeckVariantCardView M_View;

	EDeckVariantCardPlace M_Place = EDeckVariantCardPlace::Preview;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_Canvas;

	UPROPERTY()
	TObjectPtr<UImage> M_Frame;

	UPROPERTY()
	TObjectPtr<UImage> M_Picture;

	UPROPERTY()
	TObjectPtr<UBorder> M_TypeBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TypeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_FitsLabel;

	UPROPERTY()
	TObjectPtr<UImage> M_UnitPicture;

	// The variant's name along the painted spine: the only part that shows while the card is tucked behind a unit.
	UPROPERTY()
	TObjectPtr<UTextBlock> M_SpineText;
};
