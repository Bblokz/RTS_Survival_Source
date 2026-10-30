// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SurvivalArmyTypes.h"

#include "SurvivalArmyLibrary.generated.h"

class USurvivalModeSettings;

/**
 * @brief Builds the army roster for the selection UI and validates the player's picks.
 */
UCLASS()
class RTS_SURVIVAL_API USurvivalArmyLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief The faction's trainable units (training layout) filtered to the allowed unit types, each as the
	 * variant saved in the player profile and with its supply cost. The HQ entry is first and flagged bIsHQ.
	 * @param Settings Survival settings with the supply table.
	 * @param Faction The faction the player picked; NotInitialised falls back to GerBreakthroughDoctrine.
	 */
	static TArray<FSurvivalArmyEntry> BuildArmyRoster(const USurvivalModeSettings* Settings, ERTSFaction Faction);

	/** @return The HQ entry for the faction (slot 0). */
	static FSurvivalArmyEntry BuildHQEntry(const USurvivalModeSettings* Settings, ERTSFaction Faction);

	/** @return Supply cost of the unit as this variant; falls back to the per-type default, then to 10. */
	static int32 GetSupplyCost(const USurvivalModeSettings* Settings, const FTrainingOption& Unit, EUnitVariant Variant);

	/** @return Sum of supply of all picks (the HQ costs nothing). */
	static int32 GetTotalSupply(const TArray<FSurvivalArmyPick>& Picks);

	/**
	 * @brief Checks budget, slot count, max copies and that exactly one HQ is in slot 0.
	 * @param OutReason Player-facing reason when invalid.
	 */
	static bool GetIsArmyValid(const USurvivalModeSettings* Settings, const TArray<FSurvivalArmyPick>& Picks,
	                           FText& OutReason);

	/** @return The variant saved in the player profile for the unit, Base when none or not implemented. */
	static EUnitVariant GetProfileVariant(const FTrainingOption& Unit);
};
