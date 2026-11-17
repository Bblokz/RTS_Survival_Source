// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DeckBuilderTypes.h"
#include "RTS_Survival/WorldCampaign/PlayerProfile/PlayerCardData/PlayerCardData.h"

/** What one section holds: its slots are filled front to back, so Cards.Num() is the filled count. */
struct FDeckSectionState
{
	EDeckSection Section = EDeckSection::Technologies;

	// False when the player's profile has no layout for this building yet (or no technology slots).
	bool bIsAvailable = false;

	int32 SlotCount = 0;

	TArray<ERTSCard> Cards;
};

/**
 * @brief The deck the player is editing in the deck builder: which cards sit in which section, which stay in the
 * collection and which variant is clipped onto which unit. The menu only draws this and calls its edits; every rule
 * (what fits where, what a variant may attach to, what is written back to the campaign save) lives here.
 */
class RTS_SURVIVAL_API FDeckBuilderState
{
public:
	FDeckBuilderState();

	/**
	 * @brief Splits the saved card data over the sections, the collection and the variant choices.
	 * @param SaveData The campaign's card data; start-unit and resource cards are kept aside and written back unused.
	 * @param Definitions Type, unit and technology per card; a card without a definition is kept aside as well.
	 * @param OwnedVariants Every variant the player may attach (never Base).
	 */
	void Init(const FPlayerCardSaveData& SaveData, const TMap<ERTSCard, FDeckCardDefinition>& Definitions,
	          const TArray<FUnitVariantSelection>& OwnedVariants);

	const FDeckSectionState& GetSection(const EDeckSection Section) const;

	/** @return The technology and training cards that are not in a slot. */
	const TArray<ERTSCard>& GetCollection() const { return M_Collection; }

	/** @return The collection cards a slot of this section takes. */
	TArray<ERTSCard> GetCollectionForSection(const EDeckSection Section) const;

	/** @return Owned variants that are not clipped onto their unit: every owned variant of a unit that runs Base. */
	TArray<FUnitVariantSelection> GetLooseVariants() const;

	/** @return The loose variants of units whose card belongs to this section (slotted or in the collection). */
	TArray<FUnitVariantSelection> GetLooseVariantsForSection(const EDeckSection Section) const;

	/** @return Base when nothing is clipped onto the unit. */
	EUnitVariant GetAttachedVariant(const FTrainingOption& Unit) const;

	/** @return The owned variants of the unit in variant order, without Base. */
	TArray<EUnitVariant> GetOwnedVariants(const FTrainingOption& Unit) const;

	/** @return How many units in the section's slots have a variant clipped on. */
	int32 GetAttachedVariantCount(const EDeckSection Section) const;

	/** @return Null for a card the card table has no row for. */
	const FDeckCardDefinition* FindDefinition(const ERTSCard Card) const;

	/**
	 * @brief Moves a collection card into a section. An occupied slot hands its card back to the collection.
	 * @param Card A card in the collection.
	 * @param Section The section to slot it in; must take this card's type and must not already hold the card.
	 * @param SlotIndex The slot dropped on; INDEX_NONE or an empty slot appends to the filled slots.
	 * @return False when nothing changed.
	 */
	bool SlotCard(const ERTSCard Card, const EDeckSection Section, const int32 SlotIndex = INDEX_NONE);

	/** @return False when the slot is empty. */
	bool ReturnCardToCollection(const EDeckSection Section, const int32 SlotIndex);

	/** @return False when the variant is not owned or is Base. Replaces what was clipped onto the unit. */
	bool AttachVariant(const FUnitVariantSelection& Variant);

	/** @return False when the unit already runs Base. */
	bool DetachVariant(const FTrainingOption& Unit);

	/**
	 * @brief The card data to store in the campaign save. Start-unit and resource cards come back as unslotted
	 * collection cards and are left out of the selected cards, so they have no effect in a mission.
	 * @return Not normalised; pass it through FRTS_Profile::BuildPlayerCardSaveData before saving.
	 */
	FPlayerCardSaveData BuildSaveData() const;

private:
	FDeckSectionState& GetMutableSection(const EDeckSection Section);
	void Init_ResetSections();
	void Init_TechnologySection(const FPlayerCardSaveData& SaveData);
	void Init_BuildingSections(const FPlayerCardSaveData& SaveData);
	void Init_Collection(const FPlayerCardSaveData& SaveData);
	void Init_Variants(const FPlayerCardSaveData& SaveData, const TArray<FUnitVariantSelection>& OwnedVariants);

	/** Puts the saved cards a section may hold into it; the rest goes to the cards kept aside. */
	void FillSectionFromSavedCards(FDeckSectionState& Section, const TArray<ERTSCard>& SavedCards);

	bool GetIsCardOfSection(const ERTSCard Card, const EDeckSection Section) const;
	bool GetIsVariantOwned(const FUnitVariantSelection& Variant) const;

	/** @return Whether a card of this section (slotted or in the collection) is the card of Unit. */
	bool GetIsUnitOfSection(const FTrainingOption& Unit, const EDeckSection Section) const;

	void AppendSelectedCards(const FDeckSectionState& Section, TArray<FCardSaveData>& OutSelectedCards) const;

	TMap<ERTSCard, FDeckCardDefinition> M_Definitions;

	TArray<FDeckSectionState> M_Sections;

	TArray<ERTSCard> M_Collection;

	// Start-unit and resource cards and cards without a table row: not shown, not usable, written back unchanged.
	TArray<ERTSCard> M_CardsKeptAside;

	// Layouts of building types the deck builder has no section for; written back unchanged.
	TArray<FNomadicBuildingLayoutData> M_LayoutsKeptAside;

	TArray<FUnitVariantSelection> M_OwnedVariants;

	// The owned variants as the save had them, so a development grant of every variant is never written back.
	TArray<FUnitVariantSelection> M_SavedOwnedVariants;

	TMap<FTrainingOption, EUnitVariant> M_AttachedVariants;

	int32 M_MaxCardsInUnits = 0;
};
