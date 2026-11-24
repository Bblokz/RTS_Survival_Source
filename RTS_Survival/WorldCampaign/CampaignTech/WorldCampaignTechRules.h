// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechSettings.h"

/**
 * @brief Why a technology cannot be bought right now; None means it can.
 */
enum class EWorldCampaignTechResearchBlock : uint8
{
	None,
	/** Tech_NONE or a technology that has no place in the tree. */
	NotInTree,
	AlreadyResearched,
	MissingPrerequisite,
	CannotAfford
};

/**
 * @brief The world-map tech tree rules as pure functions of plain data, so the map, the UI and the tests share one
 * formula. The component UWorldCampaignTechManager only feeds them the live campaign state.
 */
namespace WorldCampaignTechRules
{
	/**
	 * @brief Prerequisites of the technology that are not researched yet, in the order the table lists them.
	 * @param Table Tree to read the prerequisites from.
	 * @param Technology Technology to check.
	 * @param ResearchedTechnologies What the campaign already owns.
	 * @return Empty when every prerequisite is owned or the technology is not in the tree.
	 */
	RTS_SURVIVAL_API TArray<ETechnology> GetMissingPrerequisites(const FWorldCampaignTechTable& Table,
	                                                             ETechnology Technology,
	                                                             const TArray<ETechnology>& ResearchedTechnologies);

	/**
	 * @brief The first reason that stops a purchase, checked in the order the player would fix them.
	 * @param Table Tree to read the definition from.
	 * @param Technology Technology the player wants.
	 * @param ResearchedTechnologies What the campaign already owns.
	 * @param Resources What the player holds now.
	 * @return None when the technology can be bought.
	 */
	RTS_SURVIVAL_API EWorldCampaignTechResearchBlock GetResearchBlock(const FWorldCampaignTechTable& Table,
	                                                                  ETechnology Technology,
	                                                                  const TArray<ETechnology>&
	                                                                  ResearchedTechnologies,
	                                                                  const FWorldMapResources& Resources);

	/**
	 * @brief The technologies of one branch, sorted by row and then by enum value so the UI can lay them out.
	 * @return Empty for EWorldCampaignTechBranch::None or a branch without technologies.
	 */
	RTS_SURVIVAL_API TArray<ETechnology> GetTechnologiesInBranch(const FWorldCampaignTechTable& Table,
	                                                             EWorldCampaignTechBranch Branch);

	/**
	 * @brief Player-facing text for a blocked purchase.
	 * @param Block Reason returned by GetResearchBlock.
	 * @param MissingPrerequisites Listed by name when the block is MissingPrerequisite.
	 * @return Empty text for EWorldCampaignTechResearchBlock::None.
	 */
	RTS_SURVIVAL_API FText GetResearchBlockText(EWorldCampaignTechResearchBlock Block,
	                                            const TArray<ETechnology>& MissingPrerequisites);

	/**
	 * @brief Table integrity check for the tests: every tech has a branch, and every prerequisite is in the tree
	 * and sits in a lower row, which also rules out cycles.
	 * @param OutProblems One line per problem found.
	 * @return true when the table has no problems.
	 */
	RTS_SURVIVAL_API bool GetIsTableConsistent(const FWorldCampaignTechTable& Table, TArray<FString>& OutProblems);
}
