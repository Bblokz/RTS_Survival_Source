// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DeckBuilderState.h"

FDeckBuilderState::FDeckBuilderState()
{
	Init_ResetSections();
}

void FDeckBuilderState::Init(const FPlayerCardSaveData& SaveData,
                             const TMap<ERTSCard, FDeckCardDefinition>& Definitions,
                             const TArray<FUnitVariantSelection>& OwnedVariants)
{
	M_Definitions = Definitions;
	M_Collection.Reset();
	M_CardsKeptAside.Reset();
	M_LayoutsKeptAside.Reset();
	M_MaxCardsInUnits = SaveData.MaxCardsInUnits;
	Init_ResetSections();
	Init_TechnologySection(SaveData);
	Init_BuildingSections(SaveData);
	Init_Collection(SaveData);
	Init_Variants(SaveData, OwnedVariants);
}

void FDeckBuilderState::Init_ResetSections()
{
	M_Sections.Reset();
	for (const EDeckSection Section : DeckBuilder::GetAllSections())
	{
		FDeckSectionState SectionState;
		SectionState.Section = Section;
		M_Sections.Add(SectionState);
	}
}

void FDeckBuilderState::Init_TechnologySection(const FPlayerCardSaveData& SaveData)
{
	FDeckSectionState& Technologies = GetMutableSection(EDeckSection::Technologies);
	Technologies.SlotCount = FMath::Max(0, SaveData.MaxCardsInTechResources);
	Technologies.bIsAvailable = Technologies.SlotCount > 0;
	// The old holder also took resource cards; those leave the slots and are kept aside.
	FillSectionFromSavedCards(Technologies, SaveData.CardsInTechResources);
}

void FDeckBuilderState::Init_BuildingSections(const FPlayerCardSaveData& SaveData)
{
	for (const FNomadicBuildingLayoutData& Layout : SaveData.NomadicBuildingLayouts)
	{
		EDeckSection Section = EDeckSection::Technologies;
		if (not DeckBuilder::GetSectionOfBuildingType(Layout.BuildingType, Section))
		{
			M_LayoutsKeptAside.Add(Layout);
			continue;
		}
		FDeckSectionState& SectionState = GetMutableSection(Section);
		SectionState.SlotCount = FMath::Max(0, Layout.Slots);
		SectionState.bIsAvailable = SectionState.SlotCount > 0;
		FillSectionFromSavedCards(SectionState, Layout.Cards);
	}
}

void FDeckBuilderState::FillSectionFromSavedCards(FDeckSectionState& Section, const TArray<ERTSCard>& SavedCards)
{
	for (const ERTSCard Card : SavedCards)
	{
		if (Card == ERTSCard::Card_Empty || Card == ERTSCard::Card_INVALID)
		{
			continue;
		}
		const bool bFitsSection = GetIsCardOfSection(Card, Section.Section)
			&& Section.Cards.Num() < Section.SlotCount
			&& not Section.Cards.Contains(Card);
		if (bFitsSection)
		{
			Section.Cards.Add(Card);
			continue;
		}
		M_CardsKeptAside.Add(Card);
	}
}

void FDeckBuilderState::Init_Collection(const FPlayerCardSaveData& SaveData)
{
	for (const ERTSCard Card : SaveData.PlayerAvailableCards)
	{
		if (Card == ERTSCard::Card_Empty || Card == ERTSCard::Card_INVALID)
		{
			continue;
		}
		EDeckSection Section = EDeckSection::Technologies;
		const FDeckCardDefinition* Definition = FindDefinition(Card);
		const bool bIsDeckCard = Definition && DeckBuilder::GetSectionOfCardType(Definition->CardType, Section);
		// Technology and training cards exist once; the profile normaliser drops their duplicates on save as well.
		if (bIsDeckCard && not M_Collection.Contains(Card))
		{
			M_Collection.Add(Card);
			continue;
		}
		if (not bIsDeckCard)
		{
			M_CardsKeptAside.Add(Card);
		}
	}
	// The start-unit holder is gone; its cards go back to the unused cards instead of being lost.
	for (const ERTSCard Card : SaveData.CardsInUnits)
	{
		if (Card != ERTSCard::Card_Empty && Card != ERTSCard::Card_INVALID)
		{
			M_CardsKeptAside.Add(Card);
		}
	}
}

void FDeckBuilderState::Init_Variants(const FPlayerCardSaveData& SaveData,
                                      const TArray<FUnitVariantSelection>& OwnedVariants)
{
	M_SavedOwnedVariants = SaveData.OwnedUnitVariants;
	M_OwnedVariants.Reset();
	for (const FUnitVariantSelection& Variant : OwnedVariants)
	{
		if (Variant.Variant != EUnitVariant::Base && not GetIsVariantOwned(Variant))
		{
			M_OwnedVariants.Add(Variant);
		}
	}
	M_AttachedVariants.Reset();
	for (const FUnitVariantSelection& Selection : SaveData.UnitVariantSelections)
	{
		if (Selection.Variant != EUnitVariant::Base && GetIsVariantOwned(Selection))
		{
			M_AttachedVariants.Add(Selection.Unit, Selection.Variant);
		}
	}
}

const FDeckSectionState& FDeckBuilderState::GetSection(const EDeckSection Section) const
{
	// Sections are created in enum order, so the enum value is the index.
	return M_Sections[static_cast<int32>(Section)];
}

FDeckSectionState& FDeckBuilderState::GetMutableSection(const EDeckSection Section)
{
	return M_Sections[static_cast<int32>(Section)];
}

TArray<ERTSCard> FDeckBuilderState::GetCollectionForSection(const EDeckSection Section) const
{
	TArray<ERTSCard> Cards;
	for (const ERTSCard Card : M_Collection)
	{
		if (GetIsCardOfSection(Card, Section))
		{
			Cards.Add(Card);
		}
	}
	return Cards;
}

TArray<FUnitVariantSelection> FDeckBuilderState::GetLooseVariants() const
{
	TArray<FUnitVariantSelection> LooseVariants;
	for (const FUnitVariantSelection& Variant : M_OwnedVariants)
	{
		if (GetAttachedVariant(Variant.Unit) == EUnitVariant::Base)
		{
			LooseVariants.Add(Variant);
		}
	}
	return LooseVariants;
}

TArray<FUnitVariantSelection> FDeckBuilderState::GetLooseVariantsForSection(const EDeckSection Section) const
{
	TArray<FUnitVariantSelection> LooseVariants;
	for (const FUnitVariantSelection& Variant : GetLooseVariants())
	{
		if (GetIsUnitOfSection(Variant.Unit, Section))
		{
			LooseVariants.Add(Variant);
		}
	}
	return LooseVariants;
}

EUnitVariant FDeckBuilderState::GetAttachedVariant(const FTrainingOption& Unit) const
{
	const EUnitVariant* AttachedVariant = M_AttachedVariants.Find(Unit);
	return AttachedVariant ? *AttachedVariant : EUnitVariant::Base;
}

TArray<EUnitVariant> FDeckBuilderState::GetOwnedVariants(const FTrainingOption& Unit) const
{
	TArray<EUnitVariant> Variants;
	for (const FUnitVariantSelection& Variant : M_OwnedVariants)
	{
		if (Variant.Unit == Unit)
		{
			Variants.Add(Variant.Variant);
		}
	}
	Variants.Sort([](const EUnitVariant Left, const EUnitVariant Right)
	{
		return static_cast<uint8>(Left) < static_cast<uint8>(Right);
	});
	return Variants;
}

int32 FDeckBuilderState::GetAttachedVariantCount(const EDeckSection Section) const
{
	int32 AttachedCount = 0;
	for (const ERTSCard Card : GetSection(Section).Cards)
	{
		const FDeckCardDefinition* Definition = FindDefinition(Card);
		if (Definition && GetAttachedVariant(Definition->VariantUnit) != EUnitVariant::Base)
		{
			AttachedCount++;
		}
	}
	return AttachedCount;
}

const FDeckCardDefinition* FDeckBuilderState::FindDefinition(const ERTSCard Card) const
{
	return M_Definitions.Find(Card);
}

bool FDeckBuilderState::SlotCard(const ERTSCard Card, const EDeckSection Section, const int32 SlotIndex)
{
	FDeckSectionState& SectionState = GetMutableSection(Section);
	const int32 CollectionIndex = M_Collection.Find(Card);
	const bool bMaySlot = SectionState.bIsAvailable
		&& CollectionIndex != INDEX_NONE
		&& GetIsCardOfSection(Card, Section)
		&& not SectionState.Cards.Contains(Card);
	if (not bMaySlot)
	{
		return false;
	}
	if (SectionState.Cards.IsValidIndex(SlotIndex))
	{
		const ERTSCard ReplacedCard = SectionState.Cards[SlotIndex];
		SectionState.Cards[SlotIndex] = Card;
		M_Collection.RemoveAt(CollectionIndex);
		M_Collection.Add(ReplacedCard);
		return true;
	}
	if (SectionState.Cards.Num() >= SectionState.SlotCount)
	{
		return false;
	}
	SectionState.Cards.Add(Card);
	M_Collection.RemoveAt(CollectionIndex);
	return true;
}

bool FDeckBuilderState::ReturnCardToCollection(const EDeckSection Section, const int32 SlotIndex)
{
	FDeckSectionState& SectionState = GetMutableSection(Section);
	if (not SectionState.Cards.IsValidIndex(SlotIndex))
	{
		return false;
	}
	M_Collection.Add(SectionState.Cards[SlotIndex]);
	SectionState.Cards.RemoveAt(SlotIndex);
	return true;
}

bool FDeckBuilderState::AttachVariant(const FUnitVariantSelection& Variant)
{
	if (Variant.Variant == EUnitVariant::Base || not GetIsVariantOwned(Variant))
	{
		return false;
	}
	M_AttachedVariants.Add(Variant.Unit, Variant.Variant);
	return true;
}

bool FDeckBuilderState::DetachVariant(const FTrainingOption& Unit)
{
	return M_AttachedVariants.Remove(Unit) > 0;
}

FPlayerCardSaveData FDeckBuilderState::BuildSaveData() const
{
	FPlayerCardSaveData SaveData;
	SaveData.PlayerAvailableCards = M_Collection;
	SaveData.PlayerAvailableCards.Append(M_CardsKeptAside);
	SaveData.MaxCardsInUnits = M_MaxCardsInUnits;
	const FDeckSectionState& Technologies = GetSection(EDeckSection::Technologies);
	SaveData.MaxCardsInTechResources = Technologies.SlotCount;
	SaveData.CardsInTechResources = Technologies.Cards;
	AppendSelectedCards(Technologies, SaveData.SelectedCards);
	for (const FDeckSectionState& Section : M_Sections)
	{
		const ENomadicLayoutBuildingType BuildingType = DeckBuilder::GetBuildingTypeOfSection(Section.Section);
		if (BuildingType == ENomadicLayoutBuildingType::Building_None || not Section.bIsAvailable)
		{
			continue;
		}
		FNomadicBuildingLayoutData Layout;
		Layout.BuildingType = BuildingType;
		Layout.Slots = Section.SlotCount;
		Layout.Cards = Section.Cards;
		SaveData.NomadicBuildingLayouts.Add(Layout);
		AppendSelectedCards(Section, SaveData.SelectedCards);
	}
	SaveData.NomadicBuildingLayouts.Append(M_LayoutsKeptAside);
	for (const TPair<FTrainingOption, EUnitVariant>& Attached : M_AttachedVariants)
	{
		FUnitVariantSelection Selection;
		Selection.Unit = Attached.Key;
		Selection.Variant = Attached.Value;
		SaveData.UnitVariantSelections.Add(Selection);
	}
	SaveData.OwnedUnitVariants = M_SavedOwnedVariants;
	return SaveData;
}

void FDeckBuilderState::AppendSelectedCards(const FDeckSectionState& Section,
                                            TArray<FCardSaveData>& OutSelectedCards) const
{
	for (const ERTSCard Card : Section.Cards)
	{
		const FDeckCardDefinition* Definition = FindDefinition(Card);
		if (not Definition)
		{
			continue;
		}
		FCardSaveData CardData;
		CardData.CardType = Definition->CardType;
		CardData.TrainingOption = Definition->TrainingOption;
		CardData.Technology = Definition->Technology;
		OutSelectedCards.Add(CardData);
	}
}

bool FDeckBuilderState::GetIsCardOfSection(const ERTSCard Card, const EDeckSection Section) const
{
	const FDeckCardDefinition* Definition = FindDefinition(Card);
	return Definition && Definition->CardType == DeckBuilder::GetCardTypeOfSection(Section);
}

bool FDeckBuilderState::GetIsVariantOwned(const FUnitVariantSelection& Variant) const
{
	return M_OwnedVariants.ContainsByPredicate([&Variant](const FUnitVariantSelection& Owned)
	{
		return DeckBuilder::GetIsSameVariant(Owned, Variant);
	});
}

bool FDeckBuilderState::GetIsUnitOfSection(const FTrainingOption& Unit, const EDeckSection Section) const
{
	const auto IsCardOfUnit = [this, &Unit, Section](const ERTSCard Card)
	{
		const FDeckCardDefinition* Definition = FindDefinition(Card);
		return Definition && Definition->VariantUnit == Unit && GetIsCardOfSection(Card, Section);
	};
	return GetSection(Section).Cards.ContainsByPredicate(IsCardOfUnit) || M_Collection.ContainsByPredicate(IsCardOfUnit);
}
