// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"

#include "CommanderSelectionData.generated.h"

class UGlobalAbilitySetDataAsset;
class UTexture2D;

/** One slot of a commander's kit as the commander selection screen shows it. */
USTRUCT()
struct FCommanderSelectionAbilitySlot
{
	GENERATED_BODY()

	EGlobalAbility Ability = EGlobalAbility::GA_None;

	// False while DA_GlobalAbilitySystem has no template for the ability; the screen then shows a placeholder.
	bool bHasTemplate = false;

	EGlobalAbilityTier Tier = EGlobalAbilityTier::TierI;

	FText Title;

	FText Description;

	// Owned by the ability template, which the screen keeps loaded with the ability data asset.
	UPROPERTY()
	TObjectPtr<UTexture2D> Icon = nullptr;
};

/** A commander as the selection screen lists it: config text and portrait plus its resolved kit. */
USTRUCT()
struct FCommanderSelectionEntryData
{
	GENERATED_BODY()

	ERTSCommander Commander = ERTSCommander::NoCommander;

	FText Title;

	FText Description;

	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY()
	TArray<FCommanderSelectionAbilitySlot> AbilitySlots;
};

namespace CommanderSelectionData
{
	/**
	 * @brief Reads UCommanderSettings and the ability templates without reporting missing templates, which are
	 * expected while new abilities are being authored.
	 * @param AbilityDataAsset DA_GlobalAbilitySystem; null gives every slot a placeholder.
	 * @return Every configured commander but NoCommander, in ERTSCommander order.
	 */
	TArray<FCommanderSelectionEntryData> BuildCommanderEntries(const UGlobalAbilitySetDataAsset* AbilityDataAsset);

	/** "I" to "IV"; the tier badges are too small for "Tier III". */
	FText GetTierNumeral(const EGlobalAbilityTier Tier);
}
