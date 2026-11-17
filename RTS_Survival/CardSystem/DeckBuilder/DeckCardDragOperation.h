// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "DeckBuilderTypes.h"

#include "DeckCardDragOperation.generated.h"

/** What a variant card shows; built by the deck builder menu from the variant data asset and the card table. */
struct FDeckVariantCardView
{
	FUnitVariantSelection Variant;

	// "VARIANT A", as the hangar screen labels it.
	FText Label;

	FText Name;

	FText Description;

	FSlateBrush Picture;

	// The unit the variant fits, shown at the foot of the card.
	FText UnitName;

	FSlateBrush UnitPicture;

	ERTSCardRarity Rarity = ERTSCardRarity::Rarity_UnCommon;
};

/** The variant cards riding on a unit card. */
struct FDeckCardVariantsView
{
	// The attached variant first, then the unit's other owned variants; empty while the unit runs Base.
	TArray<FDeckVariantCardView> ClippedVariants;

	// Owned variants that are not clipped on, which only happens while the unit runs Base.
	int32 AvailableVariantCount = 0;
};

/**
 * @brief Created by a deck card or a variant card when the player starts dragging it; the card it is dropped on
 * hands it to the deck builder menu, which decides what the drop means.
 */
UCLASS()
class RTS_SURVIVAL_API UDeckCardDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	// True for a variant card, false for a unit or technology card.
	bool bIsVariant = false;

	ERTSCard Card = ERTSCard::Card_INVALID;

	// Unit and technology cards: whether the card was picked up from a slot, and which one.
	bool bIsFromSlot = false;

	EDeckSection Section = EDeckSection::Technologies;

	int32 SlotIndex = INDEX_NONE;

	FUnitVariantSelection Variant;
};
