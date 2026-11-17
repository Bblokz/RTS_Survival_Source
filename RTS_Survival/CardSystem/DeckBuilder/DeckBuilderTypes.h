// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "RTS_Survival/CardSystem/CardRarity/CardRarity.h"
#include "RTS_Survival/CardSystem/CardUI/NomadLayoutWidget/NomadicLayoutBuilding/NomadicLayoutBuildingType.h"
#include "RTS_Survival/CardSystem/ERTSCard/ERTSCard.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/Units/Variants/UnitVariantTypes.h"

#include "DeckBuilderTypes.generated.h"

// The sections of the deck builder, in rail order: the technologies and one section per training building.
UENUM()
enum class EDeckSection : uint8
{
	Technologies,
	Barracks,
	MechanicalDepot,
	Forge,
	T2Factory,
	Airbase,
	Experimental
};

/**
 * @brief Everything the deck builder knows about one card, read once from the card data table so neither the model
 * nor the widgets need a Blueprint card widget to learn a card's type, unit or texts.
 */
USTRUCT()
struct FDeckCardDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	ERTSCard Card = ERTSCard::Card_INVALID;

	UPROPERTY()
	ECardType CardType = ECardType::Invalid;

	UPROPERTY()
	ERTSCardRarity Rarity = ERTSCardRarity::Rarity_Common;

	UPROPERTY()
	FText Title;

	UPROPERTY()
	FText Description;

	// A UPROPERTY so the picture's texture stays reachable for the garbage collector from every copy.
	UPROPERTY()
	FSlateBrush ImageBrush;

	// Training cards: the unit the card unlocks in its building.
	UPROPERTY()
	FTrainingOption TrainingOption;

	// TrainingOption keyed the way unit variants are keyed (harvesters as tanks), to match variants to this card.
	UPROPERTY()
	FTrainingOption VariantUnit;

	// Technology cards: the technology that starts the mission researched.
	UPROPERTY()
	ETechnology Technology = ETechnology::Tech_NONE;
};

namespace DeckBuilder
{
	constexpr int32 SectionCount = 7;

	/** @return Every section in rail order. */
	RTS_SURVIVAL_API TConstArrayView<EDeckSection> GetAllSections();

	/** @return The card type a section's slots take. */
	RTS_SURVIVAL_API ECardType GetCardTypeOfSection(const EDeckSection Section);

	/** @return Building_None for the technologies. */
	RTS_SURVIVAL_API ENomadicLayoutBuildingType GetBuildingTypeOfSection(const EDeckSection Section);

	/** @return False when cards of this type have no section (start units, resources, empty). */
	RTS_SURVIVAL_API bool GetSectionOfCardType(const ECardType CardType, EDeckSection& OutSection);

	/** @return False for Building_None and building types the deck builder has no section for. */
	RTS_SURVIVAL_API bool GetSectionOfBuildingType(const ENomadicLayoutBuildingType BuildingType,
	                                               EDeckSection& OutSection);

	RTS_SURVIVAL_API FText GetSectionDisplayName(const EDeckSection Section);

	/** @return What the section's cards are, shown next to its name ("light vehicles"). */
	RTS_SURVIVAL_API FText GetSectionSubtitle(const EDeckSection Section);

	/** @return True when both name the same variant of the same unit. */
	inline bool GetIsSameVariant(const FUnitVariantSelection& Left, const FUnitVariantSelection& Right)
	{
		return Left.Unit == Right.Unit && Left.Variant == Right.Variant;
	}
}
