// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRulesSettings.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/PerkSystem/PlayerRank/PlayerRanks.h"

/**
 * @brief How one enemy turn's budget is divided.
 */
struct RTS_SURVIVAL_API FWorldEnemyBudgetSplit
{
	int32 ReactionPoints = 0;
	int32 FreezePoints = 0;
};

/**
 * @brief Result of paying points into the freeze.
 */
struct RTS_SURVIVAL_API FWorldFreezeAdvance
{
	int32 NewProgressPoints = 0;
	int32 NewStage = 0;
	int32 StagesGained = 0;
};

/**
 * @brief The campaign's rules as pure functions of plain data, so they can be unit tested and reused by the map,
 * the mission side and the UI without any of them holding a second copy of a formula.
 */
namespace WorldCampaignRules
{
	RTS_SURVIVAL_API EGlobalAbilityTier GetTierCapForMissionsCompleted(const FWorldCampaignRuleNumbers& Numbers,
	                                                                  int32 MissionsCompleted);

	RTS_SURVIVAL_API ERTSPlayerRank GetRankForMissionsCompleted(const FWorldCampaignRuleNumbers& Numbers,
	                                                           int32 MissionsCompleted);

	/**
	 * @brief Alert after a successful strike; strikes nearer the wall alarm the enemy more.
	 * @param CurrentAlert Alert before the strike, 0 to 1.
	 * @param Target What was struck.
	 * @param WallProximity 0 at the player HQ, 1 at the wall.
	 * @return New alert, clamped to 0..1.
	 */
	RTS_SURVIVAL_API float GetAlertAfterStrike(const FWorldCampaignRuleNumbers& Numbers, float CurrentAlert,
	                                           EWorldOperationTarget Target, float WallProximity);

	RTS_SURVIVAL_API float GetAlertAfterQuietTurn(const FWorldCampaignRuleNumbers& Numbers, float CurrentAlert);

	/**
	 * @brief Alert is the share of the budget spent on reaction; the rest is paid into the freeze.
	 * @return Both shares; they always add up to the budget.
	 */
	RTS_SURVIVAL_API FWorldEnemyBudgetSplit SplitEnemyBudget(const FWorldCampaignRuleNumbers& Numbers, float Alert);

	RTS_SURVIVAL_API int32 GetFreezePointsNeededForStage(const FWorldCampaignRuleNumbers& Numbers, int32 StageIndex);

	/**
	 * @brief Pays points into the freeze and reports the stages that finished, never passing the final stage.
	 * @param CurrentProgressPoints Points already paid toward the current stage.
	 * @param CurrentStage Finished stages so far.
	 * @param PointsToAdd Freeze share of this turn's budget.
	 * @return New progress, new stage and how many stages were gained.
	 */
	RTS_SURVIVAL_API FWorldFreezeAdvance AdvanceFreeze(const FWorldCampaignRuleNumbers& Numbers,
	                                                   int32 CurrentProgressPoints, int32 CurrentStage,
	                                                   int32 PointsToAdd);

	RTS_SURVIVAL_API bool GetIsFreezeComplete(const FWorldCampaignRuleNumbers& Numbers, int32 Stage);

	/**
	 * @brief A country's liberation value from its targets; story missions weigh more than enemy items.
	 * @return 0..1; a country without targets counts as fully liberated.
	 */
	RTS_SURVIVAL_API float GetCountryLiberation(const FWorldCampaignRuleNumbers& Numbers, int32 StoryMissionsTotal,
	                                            int32 StoryMissionsCleared, int32 OtherTargetsTotal,
	                                            int32 OtherTargetsCleared);

	/**
	 * @brief How many of a country's build nodes are open. The first opens with the first cleared target, the rest
	 * at the configured liberation values.
	 * @return Number of unlocked nodes, never more than NodesInCountry.
	 */
	RTS_SURVIVAL_API int32 GetUnlockedBuildNodeCount(const FWorldCampaignRuleNumbers& Numbers, int32 NodesInCountry,
	                                                 int32 ClearedTargetsInCountry, float Liberation);
}
