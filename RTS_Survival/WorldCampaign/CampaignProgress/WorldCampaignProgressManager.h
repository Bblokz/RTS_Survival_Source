// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Game/RTSGameInstance/WorldMissionContext/WorldMissionContext.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/PerkSystem/PlayerRank/PlayerRanks.h"
#include "WorldCampaignProgressManager.generated.h"

struct FPrimaryReward;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldMapResourcesChanged, const FWorldMapResources&, Resources);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldOperationResolved, const FWorldPendingOperation&, Operation,
                                             bool, bWasVictory);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldRankChanged, ERTSPlayerRank, NewRank, EGlobalAbilityTier,
                                             GlobalAbilityTierCap);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldTechnologyResearched, ETechnology, Technology);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldBuildingBuilt, FName, BuildNodeId, EWorldCampaignBuilding,
                                             Building);

/** Native: an anchor's mission was won or its enemy item destroyed. Countries, fog and the enemy listen to this. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldAnchorCleared, const FGuid& /*AnchorKey*/);

/**
 * @brief What resolving the pending operation changed, so the caller can update the map and start the enemy turn.
 */
struct RTS_SURVIVAL_API FWorldOperationResolution
{
	bool bWasResolved = false;
	bool bWasVictory = false;
	FWorldPendingOperation Operation;
};

/**
 * @brief Owned by the world player controller. The single owner of FWorldCampaignProgressState at runtime: mission
 * results, map resources, rank, researched tech and buildings all change through here, and the save manager reads
 * the state from here when it saves. Sibling managers that own one sub-struct edit it through GetMutableProgressState.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldCampaignProgressManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldCampaignProgressManager();

	void InitializeForNewCampaign(ERTSCommander Commander, const FRTSGameDifficulty& Difficulty);
	void RestoreFromSave(const FWorldCampaignProgressState& SavedProgressState);

	const FWorldCampaignProgressState& GetProgressState() const { return M_ProgressState; }
	FWorldCampaignProgressState& GetMutableProgressState() { return M_ProgressState; }

	// --- Resources ---

	UFUNCTION(BlueprintPure, Category = "World Campaign|Progress")
	FWorldMapResources GetResources() const { return M_ProgressState.Resources; }

	UFUNCTION(BlueprintPure, Category = "World Campaign|Progress")
	bool GetCanAfford(const FWorldMapResources& Cost) const;

	UFUNCTION(BlueprintCallable, Category = "World Campaign|Progress")
	void AddResources(const FWorldMapResources& Amount);

	/**
	 * @brief Pays a world-map price in one step so a purchase can never be half paid.
	 * @param Cost Price to pay.
	 * @return false, with nothing taken, when the player cannot afford it.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Progress")
	bool TrySpendResources(const FWorldMapResources& Cost);

	// --- Operations ---

	/** Records the operation about to be launched; the caller saves the campaign before opening the level. */
	void BeginOperation(const FWorldPendingOperation& Operation);

	/** Drops a pending operation that never reported back (the game was closed during the mission). */
	void DiscardPendingOperation();

	/**
	 * @brief Applies a returning mission result to the pending operation: counts it, clears the target and pays
	 * the reward on a win. A loss changes nothing here; its cost is the enemy turn the caller runs next.
	 * @param MissionResult Result the mission wrote to the game instance.
	 * @param RewardOnVictory Reward of the map target, or nullptr when it has none.
	 * @param WallProximity 0 at the player HQ, 1 at the wall; scales the alert a win causes.
	 * @return What was resolved; bWasResolved is false when the result does not belong to the pending operation.
	 */
	FWorldOperationResolution ResolvePendingOperation(const FWorldMissionResult& MissionResult,
	                                                  const FPrimaryReward* RewardOnVictory, float WallProximity);

	// --- Rank and tier ---

	UFUNCTION(BlueprintPure, Category = "World Campaign|Progress")
	int32 GetMissionsCompleted() const { return M_ProgressState.MissionsCompleted; }

	UFUNCTION(BlueprintPure, Category = "World Campaign|Progress")
	ERTSPlayerRank GetPlayerRank() const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Progress")
	EGlobalAbilityTier GetGlobalAbilityTierCap() const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Progress")
	bool GetIsAnchorCleared(const FGuid& AnchorKey) const;

	// --- Technology ---

	UFUNCTION(BlueprintPure, Category = "World Campaign|Progress")
	bool GetIsTechnologyResearched(ETechnology Technology) const;

	/** Stores a bought technology; the price is the caller's business (UWorldCampaignTechManager). */
	void AddResearchedTechnology(ETechnology Technology);

	// --- Buildings ---

	UFUNCTION(BlueprintPure, Category = "World Campaign|Progress")
	EWorldCampaignBuilding GetBuildingOnNode(FName BuildNodeId) const;

	/** Stores a finished building; price and unlock checks are the caller's business (UWorldBuildNodeManager). */
	void SetBuildingOnNode(FName BuildNodeId, EWorldCampaignBuilding Building, int32 CurrentTurn);

	UFUNCTION(BlueprintPure, Category = "World Campaign|Progress")
	int32 GetBuildingCount(EWorldCampaignBuilding Building) const;

	// --- Events ---

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Progress")
	FOnWorldMapResourcesChanged OnResourcesChanged;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Progress")
	FOnWorldOperationResolved OnOperationResolved;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Progress")
	FOnWorldRankChanged OnRankChanged;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Progress")
	FOnWorldTechnologyResearched OnTechnologyResearched;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Progress")
	FOnWorldBuildingBuilt OnBuildingBuilt;

	FOnWorldAnchorCleared OnAnchorCleared;

private:
	void ApplyVictory(const FWorldPendingOperation& Operation, const FPrimaryReward* RewardOnVictory,
	                  float WallProximity);
	void GrantReward(const FPrimaryReward& Reward);
	void MarkAnchorCleared(const FGuid& AnchorKey);
	bool GetDoesResultMatchPendingOperation(const FWorldMissionResult& MissionResult) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Progress",
		meta = (AllowPrivateAccess = "true"))
	FWorldCampaignProgressState M_ProgressState;
};
