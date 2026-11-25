// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechSettings.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechWheel.h"
#include "WorldCampaignTechManager.generated.h"

class UWorldCampaignProgressManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldCampaignTechPurchased, ETechnology, Technology,
                                             const FWorldMapResources&, ResourcesLeft);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldTechWheelNodeBought, FName, NodeId);

/**
 * @brief Added to the world player controller next to UWorldCampaignProgressManager. The tech tree UI asks it what
 * can be bought and calls TryResearchTechnology; the bought tech is stored in the campaign progress state and
 * researched for free at the start of every later mission.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldCampaignTechManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldCampaignTechManager();

	/**
	 * @brief Buys a technology with world-map resources in one step: checks, pays and stores it.
	 * @param Technology Technology to buy.
	 * @param OutFailReason Player-facing reason when nothing was bought.
	 * @return true when the technology is now researched for the campaign.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Tech")
	bool TryResearchTechnology(ETechnology Technology, FText& OutFailReason);

	/**
	 * @brief Same checks as TryResearchTechnology without buying, for greying out tree entries.
	 * @param Technology Technology to check.
	 * @param OutFailReason Player-facing reason when it cannot be bought.
	 * @return true when TryResearchTechnology would succeed now.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Tech")
	bool GetCanResearchTechnology(ETechnology Technology, FText& OutFailReason) const;

	/**
	 * @brief Where the technology sits in the tree and what it costs.
	 * @param Technology Technology to look up.
	 * @param OutDefinition Untouched when the technology is not in the tree.
	 * @return false when the technology is not in the tree.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Tech")
	bool GetTechnologyDefinition(ETechnology Technology, FWorldCampaignTechDefinition& OutDefinition) const;

	/** Technologies of one branch, sorted by row and then by enum value. */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Tech")
	TArray<ETechnology> GetTechnologiesInBranch(EWorldCampaignTechBranch Branch) const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Tech")
	TArray<ETechnology> GetResearchedTechnologies() const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Tech")
	bool GetIsTechnologyResearched(ETechnology Technology) const;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Tech")
	FOnWorldCampaignTechPurchased OnTechnologyPurchased;

	// --- Tech wheel: the campaign's authority for what can be bought ---

	/**
	 * @brief Buys a tech wheel node in one step: checks path and key, pays, stores the node and, for a technology
	 * node, researches its technology for the campaign.
	 * @param NodeId Node to buy.
	 * @param OutFailReason Player-facing reason when nothing was bought.
	 * @return true when the node is now bought.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Tech Wheel")
	bool TryBuyWheelNode(FName NodeId, FText& OutFailReason);

	/**
	 * @brief Same checks as TryBuyWheelNode without buying, for the research button.
	 * @param NodeId Node to check.
	 * @param OutFailReason Player-facing reason when it cannot be bought.
	 * @return true when TryBuyWheelNode would succeed now.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Tech Wheel")
	bool GetCanBuyWheelNode(FName NodeId, FText& OutFailReason) const;

	/** Bought (the start node always is), reachable through a bought neighbour, or locked. */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Tech Wheel")
	EWorldTechWheelNodeState GetWheelNodeState(FName NodeId) const;

	/** A copy of every node of the campaign's wheel. */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Tech Wheel")
	TArray<FWorldTechWheelNode> GetWheelNodes() const;

	/** The wheel the campaign uses, without copying. */
	const FWorldTechWheel& GetWheel() const;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Tech Wheel")
	FOnWorldTechWheelNodeBought OnWheelNodeBought;

protected:
	virtual void BeginPlay() override;

private:
	// Sibling component on the controller that owns the campaign progress state; the state is read through it on
	// every call so a restored save is always seen.
	UPROPERTY()
	TWeakObjectPtr<UWorldCampaignProgressManager> M_ProgressManager;

	bool GetIsValidProgressManager() const;
};
