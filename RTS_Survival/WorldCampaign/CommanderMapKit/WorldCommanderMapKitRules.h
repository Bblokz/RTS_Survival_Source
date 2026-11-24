// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKitSettings.h"

/**
 * @brief Why the map ability cannot be used right now; None means it can.
 */
enum class EWorldCommanderAbilityBlock : uint8
{
	None,
	NoCommander,
	NoAbility,
	OnCooldown,
	MissingTarget,
	CannotAfford
};

/**
 * @brief Result of a radixite to blueprint conversion.
 */
struct RTS_SURVIVAL_API FWorldCommanderConversion
{
	int32 RadixiteSpent = 0;
	int32 BlueprintsGained = 0;
};

/**
 * @brief The commander map kit rules as pure functions of plain data, so the map, the UI and the tests share one
 * formula. UWorldCommanderMapKit only feeds them the live campaign state.
 */
namespace WorldCommanderMapKitRules
{
	RTS_SURVIVAL_API EWorldCommanderAbilityTargetKind GetRequiredTargetKind(EWorldCommanderMapAbility Ability);

	RTS_SURVIVAL_API bool GetIsTargetValidForAbility(EWorldCommanderMapAbility Ability,
	                                                 const FWorldCommanderMapAbilityTarget& Target);

	/**
	 * @brief How much radixite the ability turns into blueprints with what the player holds now.
	 * @param Ability Conversion definition: Magnitude radixite per blueprint, Resources.Blueprints per-use cap.
	 * @param RadixiteHeld Radixite the player holds.
	 * @return Zero blueprints when not even one can be paid for.
	 */
	RTS_SURVIVAL_API FWorldCommanderConversion GetConversion(const FWorldCommanderMapAbilityDefinition& Ability,
	                                                         int32 RadixiteHeld);

	/**
	 * @brief The first reason that stops the map ability, checked in the order the player would fix them.
	 * @param MapState Commander, cooldown and effects of the campaign.
	 * @param Kit The commander's kit, or nullptr when the commander has none.
	 * @param Target Where the player aimed.
	 * @param Resources What the player holds now.
	 * @return None when the ability can be used.
	 */
	RTS_SURVIVAL_API EWorldCommanderAbilityBlock GetAbilityBlock(const FWorldCommanderMapState& MapState,
	                                                             const FWorldCommanderMapKitDefinition* Kit,
	                                                             const FWorldCommanderMapAbilityTarget& Target,
	                                                             const FWorldMapResources& Resources);

	RTS_SURVIVAL_API FText GetAbilityBlockText(EWorldCommanderAbilityBlock Block);

	/**
	 * @brief Stores the ability's cooldown and, for an effect that lasts, a timed effect. Call after the effect ran.
	 * @param MapState State to change.
	 * @param Ability The ability that was used.
	 * @param Target Where it was aimed; copied into the timed effect.
	 */
	RTS_SURVIVAL_API void RecordAbilityUse(FWorldCommanderMapState& MapState,
	                                       const FWorldCommanderMapAbilityDefinition& Ability,
	                                       const FWorldCommanderMapAbilityTarget& Target);

	/**
	 * @brief Start of a player turn: one turn off the cooldown and off every timed effect.
	 * @param MapState State to change.
	 * @return The effects that ended now, so their systems can undo them.
	 */
	RTS_SURVIVAL_API TArray<FWorldCommanderTimedEffect> TickStartOfPlayerTurn(FWorldCommanderMapState& MapState);

	/** Value of the passive when it is of Type and applies to Building, else Neutral. */
	RTS_SURVIVAL_API float GetPassiveBuildingValue(const FWorldCommanderPassiveDefinition& Passive,
	                                               EWorldCommanderPassive Type, EWorldCampaignBuilding Building,
	                                               float Neutral);

	/**
	 * @brief Scales the resources the scope names and rounds to whole units; the others stay as they are.
	 * @param Resources Amount to scale.
	 * @param Multiplier Applied to the scoped resources; negative values count as 0.
	 * @param Scope Which resources to scale.
	 * @return The scaled amount.
	 */
	RTS_SURVIVAL_API FWorldMapResources ScaleResources(const FWorldMapResources& Resources, float Multiplier,
	                                                   EWorldMapResourceScope Scope);

	/**
	 * @brief Operation reward after the passive, when it is a reward passive that applies to this operation.
	 * @param Passive The commander's passive.
	 * @param BaseReward Reward before the passive.
	 * @param Target What kind of operation was won.
	 * @param bIsStoryMission Pinned story missions can be excluded by the passive.
	 * @return BaseReward unchanged when the passive does not apply.
	 */
	RTS_SURVIVAL_API FWorldMapResources GetScaledOperationReward(const FWorldCommanderPassiveDefinition& Passive,
	                                                             const FWorldMapResources& BaseReward,
	                                                             EWorldOperationTarget Target,
	                                                             bool bIsStoryMission);
}
