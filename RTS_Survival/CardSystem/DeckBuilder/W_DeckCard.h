// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeckCardDragOperation.h"

#include "W_DeckCard.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class USizeBox;
class UTextBlock;
class UW_DeckBuilderMenu;
class UW_DeckVariantCard;
struct FDeckBuilderLook;

// Where a unit or technology card is shown, which decides what a click, a drag or a drop on it does.
UENUM()
enum class EDeckCardPlace : uint8
{
	// In the collection: drag it onto a slot, or right-click to put it in the open section.
	Collection,
	// In a slot of a section (filled or empty): right-click sends the card back; a card dropped on it takes the slot.
	Slot,
	// The dock and the picture that follows the cursor during a drag: no input.
	Preview
};

/**
 * @brief A unit or technology card, or an empty slot, created by the deck builder menu. The unit card is the card
 * art as it always was; a variant rides behind it as a narrow card whose spine shows past the right edge. Builds its
 * own widget tree; the menu's look supplies the art.
 */
UCLASS()
class RTS_SURVIVAL_API UW_DeckCard : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the card.
	 * @param Menu Receives the hover, right-click and drop of this card.
	 * @param Look The menu's art.
	 * @param Definition Texts, picture, type and rarity of the card.
	 * @param Variants The variant cards clipped onto the unit and how many more wait in the collection.
	 * @param Place Where the card is shown.
	 * @param Section The section of the slot; for a collection card, the section its type belongs to.
	 * @param SlotIndex The slot the card sits in; INDEX_NONE outside a slot.
	 */
	void SetupCard(UW_DeckBuilderMenu* Menu, const FDeckBuilderLook& Look, const FDeckCardDefinition& Definition,
	               const FDeckCardVariantsView& Variants, const EDeckCardPlace Place, const EDeckSection Section,
	               const int32 SlotIndex);

	/** Shows the vacant frame; the slot still takes a card dropped on it. */
	void SetupEmptySlot(UW_DeckBuilderMenu* Menu, const FDeckBuilderLook& Look, const EDeckSection Section,
	                    const int32 SlotIndex);

	bool GetIsEmptySlot() const { return bM_IsEmptySlot; }
	ERTSCard GetCard() const { return M_Definition.Card; }
	const FDeckCardDefinition& GetDefinition() const { return M_Definition; }
	EDeckCardPlace GetPlace() const { return M_Place; }
	EDeckSection GetSection() const { return M_Section; }
	int32 GetSlotIndex() const { return M_SlotIndex; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
	                                  UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	                               UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	                          UDragDropOperation* InOperation) override;

private:
	void BuildWidgetTree();
	void BuildWidgetTree_Art();
	void BuildWidgetTree_Texts();
	void SetupCard_Art(const FDeckBuilderLook& Look, const bool bIsTechnology);
	void SetupCard_Texts(const FDeckBuilderLook& Look, const bool bIsTechnology);
	void SetupCard_Variants(const FDeckBuilderLook& Look, const FDeckCardVariantsView& Variants);
	void ApplyTextStyle(const FDeckBuilderLook& Look);
	void SetDropHighlight(const bool bIsHighlighted);
	bool GetIsValidMenu() const;

	UPROPERTY()
	TWeakObjectPtr<UW_DeckBuilderMenu> M_Menu;

	UPROPERTY()
	FDeckCardDefinition M_Definition;

	FDeckCardVariantsView M_Variants;

	EDeckCardPlace M_Place = EDeckCardPlace::Preview;

	EDeckSection M_Section = EDeckSection::Technologies;

	int32 M_SlotIndex = INDEX_NONE;

	bool bM_IsEmptySlot = false;

	UPROPERTY()
	TObjectPtr<USizeBox> M_RootBox;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_Canvas;

	// Holds the variant cards clipped onto this unit; drawn under the unit card so only their spines show.
	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_SpineCanvas;

	UPROPERTY()
	TArray<TObjectPtr<UW_DeckVariantCard>> M_ClippedVariantCards;

	UPROPERTY()
	TObjectPtr<UImage> M_Frame;

	UPROPERTY()
	TObjectPtr<UImage> M_Picture;

	UPROPERTY()
	TObjectPtr<UImage> M_BuildingWell;

	UPROPERTY()
	TObjectPtr<UImage> M_BuildingIcon;

	UPROPERTY()
	TObjectPtr<UBorder> M_TypeBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TypeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_DescriptionText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_RarityText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_EmptySlotText;

	// The lane where a variant goes, shown while the unit owns variants but runs Base.
	UPROPERTY()
	TObjectPtr<UBorder> M_VariantLane;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_VariantLaneText;

	// Shown while a card that may be dropped here is dragged over this card.
	UPROPERTY()
	TObjectPtr<UBorder> M_DropHighlight;
};
