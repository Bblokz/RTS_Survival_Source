// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeTypes.h"

/**
 * @brief The unlock order key of one build node of a country.
 */
struct RTS_SURVIVAL_API FWorldBuildNodeUnlockEntry
{
	FName BuildNodeId = NAME_None;
	int32 UnlockOrder = 0;
};

/**
 * @brief Build node rules as pure functions of plain data so unlock order, prices and income are unit tested and
 * the manager holds no second copy of a formula.
 */
namespace WorldBuildNodeRules
{
	/**
	 * @brief Orders a country's nodes by UnlockOrder, ties broken by the node id's text so the order is the same in
	 * every run.
	 * @param NodesOfCountry Nodes of one country, in any order.
	 * @return Node ids, first to unlock first.
	 */
	RTS_SURVIVAL_API TArray<FName> GetNodesInUnlockOrder(const TArray<FWorldBuildNodeUnlockEntry>& NodesOfCountry);

	/**
	 * @brief The first UnlockedNodeCount nodes of the unlock order.
	 * @param NodesOfCountry Nodes of one country, in any order.
	 * @param UnlockedNodeCount From WorldCampaignRules::GetUnlockedBuildNodeCount; clamped to the node count.
	 * @return Ids of the unlocked nodes, first to unlock first.
	 */
	RTS_SURVIVAL_API TArray<FName> GetUnlockedNodeIds(const TArray<FWorldBuildNodeUnlockEntry>& NodesOfCountry,
	                                                  int32 UnlockedNodeCount);

	/**
	 * @brief Node state after an unlock pass. A node with a building is Built and an unlocked node never locks again.
	 * @param CurrentState State before the pass.
	 * @param bIsInUnlockedSet The node is among the country's unlocked nodes now.
	 * @param Building Building stored for the node, None when empty.
	 * @return New state.
	 */
	RTS_SURVIVAL_API EWorldBuildNodeState GetNodeStateAfterUnlockPass(EWorldBuildNodeState CurrentState,
	                                                                 bool bIsInUnlockedSet,
	                                                                 EWorldCampaignBuilding Building);

	/**
	 * @brief Checks a build request in the order the player would want to hear about it.
	 * @param NodeState Current node state.
	 * @param ExistingBuilding Building already stored for the node.
	 * @param Building Requested building.
	 * @param Cost Price of the requested building, or nullptr when it has none configured.
	 * @param Resources What the player holds.
	 * @return Success, or the first reason the request is refused.
	 */
	RTS_SURVIVAL_API EWorldBuildNodeBuildResult GetBuildResult(EWorldBuildNodeState NodeState,
	                                                           EWorldCampaignBuilding ExistingBuilding,
	                                                           EWorldCampaignBuilding Building,
	                                                           const FWorldMapResources* Cost,
	                                                           const FWorldMapResources& Resources);

	RTS_SURVIVAL_API FText GetBuildResultText(EWorldBuildNodeBuildResult BuildResult);

	/**
	 * @brief Every building that has a price, in enum order; a building type without a price cannot be built.
	 * @param BuildingCosts Prices from the campaign rules.
	 * @return Buildable types, None excluded.
	 */
	RTS_SURVIVAL_API TArray<EWorldCampaignBuilding> GetBuildOptions(
		const TMap<EWorldCampaignBuilding, FWorldMapResources>& BuildingCosts);

	/**
	 * @brief Income of all paying buildings for one turn.
	 * @param IncomePerBuilding What one paying building delivers.
	 * @param PayingBuildingCount Number of paying buildings; negative counts as zero.
	 * @return Total income.
	 */
	RTS_SURVIVAL_API FWorldMapResources GetTurnIncome(const FWorldMapResources& IncomePerBuilding,
	                                                  int32 PayingBuildingCount);

	/** @return The signed contribution of a player support building: always zero or negative. */
	RTS_SURVIVAL_API int32 GetPlayerSupportStrengthPercent(int32 EnemyStrengthReductionPercent);

	RTS_SURVIVAL_API bool GetIsWithinSupportRadiusXY(const FVector& SourceLocation, const FVector& TargetLocation,
	                                                 float XYRadius);

	/**
	 * @brief Stable source key of a build node for strength contributions, so a node's support is recognised and
	 * never added twice to the same target.
	 * @return The same FGuid for the same id in every run.
	 */
	RTS_SURVIVAL_API FGuid GetBuildNodeSourceKey(FName BuildNodeId);
}
