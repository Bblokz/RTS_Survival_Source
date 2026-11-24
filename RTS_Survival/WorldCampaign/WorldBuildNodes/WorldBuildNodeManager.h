// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeRules.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeTypes.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldMapFowTypes.h"
#include "WorldBuildNodeManager.generated.h"

class AAnchorPoint;
class AGeneratorWorldCampaign;
class AWorldBuildNode;
class AWorldFowManager;
class AWorldPlayerController;
class UWorldCampaignProgressManager;
class UWorldCountryOccupationRegulator;
struct FWorldBuildingEffectDefinition;
struct FWorldBuildNodeDefinition;
struct FWorldStrengthReason;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldBuildNodeUnlocked, FName, BuildNodeId, FName, CountryId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldBuildNodeBuildingPlaced, FName, BuildNodeId,
                                             EWorldCampaignBuilding, Building);

/**
 * @brief Owned by the world player controller. Finds hand-placed build nodes, spawns the configured ones, restores
 * their buildings from the campaign progress, unlocks nodes as countries are freed, sells buildings and applies
 * what the buildings do (fog reveal, turn income, lower enemy strength).
 * Call InitializeBuildNodes once the generated map, the progress state and the country regulator are ready.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldBuildNodeManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldBuildNodeManager();

	/**
	 * @brief Gathers and spawns the nodes, restores their buildings, binds to the country regulator, computes the
	 * unlocks and pushes the watch posts into the fog. Safe to call again; nodes are never spawned twice.
	 * @param WorldPlayerController Controller that owns the progress manager and the world generator.
	 */
	void InitializeBuildNodes(AWorldPlayerController* WorldPlayerController);

	/**
	 * @brief Builds on an unlocked, empty node when the player can pay. Pays, stores the building in the campaign
	 * progress and applies its effect right away (fog and enemy strength).
	 * @param BuildNodeId Node to build on.
	 * @param Building Building to put up.
	 * @param OutFailReason Player-facing reason when the build is refused; empty on success.
	 * @return true when the building was built.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Build Nodes")
	bool TryBuildOnNode(FName BuildNodeId, EWorldCampaignBuilding Building, FText& OutFailReason);

	/** @return Buildings that can be ordered on the node now; empty when the node is locked, built or unknown. */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Build Nodes")
	TArray<EWorldCampaignBuilding> GetBuildOptionsForNode(FName BuildNodeId) const;

	/** @return Price from the campaign rules; zero when the building has no price (it then cannot be built). */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Build Nodes")
	FWorldMapResources GetBuildingCost(EWorldCampaignBuilding Building) const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Build Nodes")
	AWorldBuildNode* GetBuildNode(FName BuildNodeId) const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Build Nodes")
	TArray<AWorldBuildNode*> GetBuildNodes() const;

	/**
	 * @brief Pays every income building (supply depots) once. Call at the start of each player turn; a second call
	 * in the same turn pays nothing.
	 */
	void ApplyStartOfPlayerTurnIncome();

	/**
	 * @brief Adds a negative strategic-support contribution from every airfield and battery to each uncleared enemy
	 * item and mission in range. Call right after AGeneratorWorldCampaign::AdjustDifficultyPercentagesForStrategicSupport;
	 * calling it twice adds nothing twice.
	 */
	void ApplyPlayerBuildingSupport();

	/**
	 * @brief Re-runs the generator's strategic support pass and then ApplyPlayerBuildingSupport, with the campaign's
	 * saved difficulty. Used after a support building is built mid-turn.
	 */
	void RefreshStrategicSupportWithPlayerBuildings();

	/** @return One reveal circle per built watch post, keyed by its node id. */
	TArray<FWorldFowRevealSource> GetWatchPostRevealSources() const;

	/** Pushes the watch posts into the fog manager (found in the world) and refreshes the fog. */
	void ApplyWatchPostFowReveal();

	/** Re-reads every country's liberation and unlocks nodes; nodes never lock again. */
	void RecomputeBuildNodeUnlocks();

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Build Nodes")
	FOnWorldBuildNodeUnlocked OnBuildNodeUnlocked;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Build Nodes")
	FOnWorldBuildNodeBuildingPlaced OnBuildNodeBuildingPlaced;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleCountryLiberationChanged(FName CountryId, float Liberation);

	void InitializeBuildNodes_GatherPlacedNodes();
	void InitializeBuildNodes_SpawnConfigNodes();
	void InitializeBuildNodes_RestoreBuildings();
	void InitializeBuildNodes_BindToCountryRegulator();
	void UnbindFromCountryRegulator();

	/** @return false when the node has no id or its id is already taken. */
	bool RegisterBuildNode(AWorldBuildNode* BuildNode);

	AWorldBuildNode* SpawnBuildNodeFromDefinition(const FWorldBuildNodeDefinition& BuildNodeDefinition);

	/**
	 * @brief Applies one country's unlock count to its nodes in unlock order.
	 * @param CountryId Country of the nodes.
	 * @param NodesOfCountry Unlock keys of every node in the country.
	 */
	void RecomputeUnlocksForCountry(FName CountryId, const TArray<FWorldBuildNodeUnlockEntry>& NodesOfCountry);

	/**
	 * @brief Weakens every uncleared enemy item and mission in range of one support building.
	 * @param BuildNode Node holding the building.
	 * @param BuildingEffect Effect of the building on the node.
	 * @param CachedAnchors Anchors of the generated map.
	 */
	void ApplySupportFromNode(const AWorldBuildNode& BuildNode, const FWorldBuildingEffectDefinition& BuildingEffect,
	                          const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors) const;

	/**
	 * @brief Adds the support contribution to one anchor's object unless it is not a target, cleared, out of range
	 * or already carries this node's contribution.
	 * @param AnchorPoint Anchor whose promoted object may be weakened.
	 * @param SourceLocation Location of the support building.
	 * @param XYRadius Support radius.
	 * @param SourceKey Stable key of the build node.
	 * @param StrengthReason Negative strength reason to add.
	 */
	void ApplySupportToAnchorTarget(const AAnchorPoint* AnchorPoint, const FVector& SourceLocation, float XYRadius,
	                                const FGuid& SourceKey, const FWorldStrengthReason& StrengthReason) const;

	void ApplyBuildingEffectsAfterBuild(EWorldCampaignBuilding Building);
	int32 GetCurrentTurn() const;

	/** @return The fog manager, found once in the world; nullptr when none was spawned. */
	AWorldFowManager* GetWorldFowManager();

	bool GetIsValidWorldPlayerController() const;
	bool GetIsValidCampaignProgressManager() const;
	bool GetIsValidCountryRegulator() const;
	bool GetIsValidWorldGenerator() const;

	/** Class spawned for nodes that exist only in the BuildNodes config; a Blueprint child can give them a look. */
	UPROPERTY(EditAnywhere, Category = "World Campaign|Build Nodes", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AWorldBuildNode> M_BuildNodeClass;

	UPROPERTY()
	TMap<FName, TWeakObjectPtr<AWorldBuildNode>> M_BuildNodesById;

	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_WorldPlayerController;

	UPROPERTY()
	TWeakObjectPtr<UWorldCampaignProgressManager> M_CampaignProgressManager;

	UPROPERTY()
	TWeakObjectPtr<UWorldCountryOccupationRegulator> M_CountryRegulator;

	UPROPERTY()
	TWeakObjectPtr<AGeneratorWorldCampaign> M_WorldGenerator;

	UPROPERTY()
	TWeakObjectPtr<AWorldFowManager> M_WorldFowManager;

	// Turn on which depots last paid, so a repeated start-of-turn call cannot pay twice. Not saved.
	int32 M_LastIncomePaidTurn = INDEX_NONE;
};
