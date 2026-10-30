// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurvivalRewardTypes.h"

class USurvivalModeSettings;

/**
 * @brief Rolls the reinforcement sets offered after a survived wave from the faction roster (selected variants
 * and supply costs included) plus the designer-authored sets.
 */
struct RTS_SURVIVAL_API FSurvivalRewardLibrary
{
	/**
	 * @brief Builds Rewards.OptionsPerReward options with different themes, each filled to the round's supply
	 * worth with random roster units; super-heavies respect their MaxCopies.
	 * @param Settings Survival settings (supply table, reward settings, deployment squad lists).
	 * @param Faction The player's faction.
	 * @param RoundNumber The round just survived (1-based).
	 * @param Random Match stream so rolls differ per match.
	 */
	static TArray<FSurvivalRewardOption> BuildRewardOptions(const USurvivalModeSettings* Settings, ERTSFaction Faction,
	                                                        int32 RoundNumber, FRandomStream& Random);

	/** @return Supply worth of each set offered after this round. */
	static int32 GetRewardSupply(const USurvivalModeSettings* Settings, int32 RoundNumber);

	static FText GetThemeTitle(ESurvivalRewardTheme Theme);

private:
	static bool GetDoesEntryFitTheme(const USurvivalModeSettings* Settings, const FSurvivalArmyEntry& Entry,
	                                 ESurvivalRewardTheme Theme);
	static FSurvivalRewardOption RollThemedOption(const USurvivalModeSettings* Settings,
	                                              const TArray<FSurvivalArmyEntry>& Roster,
	                                              ESurvivalRewardTheme Theme, int32 SupplyWorth,
	                                              FRandomStream& Random);
	static bool TryBuildAuthoredOption(const USurvivalModeSettings* Settings, const TArray<FSurvivalArmyEntry>& Roster,
	                                   int32 RoundNumber, FRandomStream& Random, FSurvivalRewardOption& OutOption);
};
