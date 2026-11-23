// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechSettings.h"
#include "WorldCampaignTechManager.generated.h"

class UWorldCampaignProgressManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldCampaignTechPurchased, ETechnology, Technology,
                                             const FWorldMapResources&, ResourcesLeft);

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

protected:
	virtual void BeginPlay() override;

private:
	// Sibling component on the controller that owns the campaign progress state; the state is read through it on
	// every call so a restored save is always seen.
	UPROPERTY()
	TWeakObjectPtr<UWorldCampaignProgressManager> M_ProgressManager;

	bool GetIsValidProgressManager() const;
};
