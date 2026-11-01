// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SurvivalArmyTypes.h"

#include "SurvivalArmyLibrary.generated.h"

class ARTSAsyncSpawner;
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
	 * @param AsyncSpawner When given, units the spawner has no class for are left out (the training layout lists a
	 * few that cannot be spawned).
	 */
	static TArray<FSurvivalArmyEntry> BuildArmyRoster(const USurvivalModeSettings* Settings, ERTSFaction Faction,
	                                                  const ARTSAsyncSpawner* AsyncSpawner = nullptr);

	/** @return The HQ entry for the faction (slot 0). */
	static FSurvivalArmyEntry BuildHQEntry(const USurvivalModeSettings* Settings, ERTSFaction Faction);

	/**
	 * @brief Every locked entry of the army in slot order: the HQ first, then each locked start unit (harvesters,
	 * scavengers) once per copy. All flagged bIsLocked and free of supply.
	 * @param Settings Survival settings with the locked start units.
	 * @param Faction The player's faction; factions without an entry use DefaultLockedStartUnits.
	 * @return Element i belongs in slot i.
	 */
	static TArray<FSurvivalArmyEntry> BuildLockedEntries(const USurvivalModeSettings* Settings, ERTSFaction Faction);

	/** @return The locked entries as picks in slots 0..N-1, ready to be extended with the player's units. */
	static TArray<FSurvivalArmyPick> BuildLockedPicks(const USurvivalModeSettings* Settings, ERTSFaction Faction);

	/** @return All army slots: SlotCount plus one extra slot per locked start unit, so free slots stay SlotCount - 1. */
	static int32 GetTotalSlotCount(const USurvivalModeSettings* Settings, ERTSFaction Faction);

	/** @return Supply cost of the unit as this variant; falls back to the per-type default, then to 10. */
	static int32 GetSupplyCost(const USurvivalModeSettings* Settings, const FTrainingOption& Unit, EUnitVariant Variant);

	/** @return Sum of supply of all picks (the HQ and the locked start units cost nothing). */
	static int32 GetTotalSupply(const TArray<FSurvivalArmyPick>& Picks);

	/**
	 * @brief Checks budget, slot count, max copies and that the locked entries (HQ in slot 0, then the locked start
	 * units) fill the first slots exactly as BuildLockedEntries lists them.
	 * @param Settings Survival settings.
	 * @param Faction The player's faction, for the HQ and the locked start units.
	 * @param Picks The army, one pick per filled slot.
	 * @param OutReason Player-facing reason when invalid.
	 */
	static bool GetIsArmyValid(const USurvivalModeSettings* Settings, ERTSFaction Faction,
	                           const TArray<FSurvivalArmyPick>& Picks, FText& OutReason);

	/** @return The variant saved in the player profile for the unit, Base when none or not implemented. */
	static EUnitVariant GetProfileVariant(const FTrainingOption& Unit);
};
