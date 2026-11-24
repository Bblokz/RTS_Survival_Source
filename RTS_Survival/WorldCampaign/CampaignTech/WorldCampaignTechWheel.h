// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechSettings.h"
#include "WorldCampaignTechWheel.generated.h"

/**
 * @brief How a tech wheel node looks to the player right now.
 */
UENUM(BlueprintType)
enum class EWorldTechWheelNodeState : uint8
{
	Bought,
	// Not bought, but a path neighbour is: the next step on the path.
	Reachable,
	Locked
};

/**
 * @brief Why a tech wheel node cannot be bought right now; None means it can.
 */
UENUM(BlueprintType)
enum class EWorldTechWheelBuyBlock : uint8
{
	None,
	AlreadyBought,
	NotReachable,
	MissingRequiredTechnology,
	CannotAfford,
	UnknownNode
};

/**
 * @brief The campaign tech wheel: the default layout of UI sketch sheet 13 and the buy rules as pure functions of
 * plain data, so the manager, the wheel screen and the tests share one formula. The path decides how a node is
 * reached; a node's RequiredTechnology is a key from any branch. The start node counts as bought from the beginning
 * and is never stored in the save.
 */
namespace WorldCampaignTechWheel
{
	/**
	 * @brief The wheel of sheet 13: five sectors of 72 degrees (infantry, light armour, heavy armour, support,
	 * economy clockwise from the top), 12 nodes per sector and two bridge pips into the next sector.
	 */
	RTS_SURVIVAL_API FWorldTechWheel MakeDefaultWheel();

	/** The five branches in sector order, clockwise from the top. */
	RTS_SURVIVAL_API TArray<EWorldCampaignTechBranch> GetSectorBranches();

	/**
	 * @brief Screen angle of a branch's sector centre in the wheel layout, for drawing the sector behind its nodes.
	 * @param Branch Branch to look up.
	 * @param OutCentreDegrees Untouched when the branch has no sector.
	 * @return false for EWorldCampaignTechBranch::None.
	 */
	RTS_SURVIVAL_API bool GetSectorCentreDegrees(EWorldCampaignTechBranch Branch, float& OutCentreDegrees);

	RTS_SURVIVAL_API float GetSectorHalfWidthDegrees();

	/**
	 * @brief Whether the node is owned: the start node always is, any other node when its id is in the save.
	 * @param Wheel Wheel the node is on.
	 * @param NodeId Node to check.
	 * @param BoughtNodeIds Bought node ids of the campaign.
	 * @return false for an unknown node.
	 */
	RTS_SURVIVAL_API bool GetIsNodeBought(const FWorldTechWheel& Wheel, FName NodeId,
	                                      const TArray<FName>& BoughtNodeIds);

	/**
	 * @brief Bought, reachable through a bought path neighbour, or locked.
	 * @param Wheel Wheel the node is on.
	 * @param NodeId Node to check.
	 * @param BoughtNodeIds Bought node ids of the campaign.
	 * @return Locked for an unknown node.
	 */
	RTS_SURVIVAL_API EWorldTechWheelNodeState GetNodeState(const FWorldTechWheel& Wheel, FName NodeId,
	                                                       const TArray<FName>& BoughtNodeIds);

	/**
	 * @brief The first reason that stops a purchase, checked in the order the player would fix them.
	 * @param Wheel Wheel the node is on.
	 * @param NodeId Node the player wants.
	 * @param BoughtNodeIds Bought node ids of the campaign.
	 * @param ResearchedTechnologies Technologies the campaign owns; the node's key must be among them.
	 * @param Resources What the player holds now.
	 * @return None when the node can be bought.
	 */
	RTS_SURVIVAL_API EWorldTechWheelBuyBlock GetNodeBuyBlock(const FWorldTechWheel& Wheel, FName NodeId,
	                                                         const TArray<FName>& BoughtNodeIds,
	                                                         const TArray<ETechnology>& ResearchedTechnologies,
	                                                         const FWorldMapResources& Resources);

	/**
	 * @brief Player-facing text for a blocked purchase.
	 * @param Block Reason returned by GetNodeBuyBlock.
	 * @param Node The node, to name its key; may be null.
	 * @return Empty text for EWorldTechWheelBuyBlock::None.
	 */
	RTS_SURVIVAL_API FText GetNodeBuyBlockText(EWorldTechWheelBuyBlock Block, const FWorldTechWheelNode* Node);

	/**
	 * @brief The bought path neighbour the node is reached through, preferring a technology over a pip.
	 * @return NAME_None when no neighbour is bought.
	 */
	RTS_SURVIVAL_API FName GetBoughtNeighbour(const FWorldTechWheel& Wheel, FName NodeId,
	                                          const TArray<FName>& BoughtNodeIds);

	/** @return The name the wheel shows for a branch: the Air branch is SUPPORT. */
	RTS_SURVIVAL_API FText GetBranchDisplayName(EWorldCampaignTechBranch Branch);

	/** @return The display name from the wheel presentation, or the technology's code name when it has none. */
	RTS_SURVIVAL_API FText GetTechnologyDisplayName(ETechnology Technology);

	/**
	 * @brief Integrity check for the tests and the settings: one start node, unique ids, symmetric links to existing
	 * nodes, every technology at most once and only on technology nodes, keys that exist on the wheel, and every
	 * node reachable from the start.
	 * @param OutProblems One line per problem found.
	 * @return true when the wheel has no problems.
	 */
	RTS_SURVIVAL_API bool GetIsWheelConsistent(const FWorldTechWheel& Wheel, TArray<FString>& OutProblems);
}
