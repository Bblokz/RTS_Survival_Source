// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/RTSGameInstance/GameInstCampaignGenerationSettings/GameInstCampaignGenerationSettings.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/GenerationStep/Enum_CampaignGenerationStep.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/EnemyHQPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/EnemyWallPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/EnemyPlacementRules/EnemyPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/MissionPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/NeutralItemPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/WorldCampaignCountDifficultyTuning.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/WorldCampaignPlacementFailurePolicy.h"

#include "WorldCampaignAsyncPlacement.generated.h"

/**
 * @brief Runtime caches shared by the generator and the async solver.
 * @note Kept reflected so designers can inspect placement decisions after generation.
 */
USTRUCT(BlueprintType)
struct FWorldCampaignDerivedData
{
	GENERATED_BODY()

	/**
	 * @note Used in: PlayerHQPlaced, EnemyObjectsPlaced, NeutralObjectsPlaced, MissionsPlaced.
	 * @note Why: Stores hop distances from the player HQ to every anchor as a reusable cache.
	 * @note Technical: Built after connections are generated; reused for hop-based filtering.
	 * @note Notes: Deterministic given seed and connection graph; treated as authoritative distances.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, int32> PlayerHQHopDistancesByAnchorKey;

	/**
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Caches hop distances from the enemy HQ for enemy spacing rules.
	 * @note Technical: Built after EnemyHQPlaced; used for Min/Max hop filters.
	 * @note Notes: Hard filter source; if missing or empty, enemy placement can fail.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, int32> EnemyHQHopDistancesByAnchorKey;

	/**
	 * @note Used in: PlayerHQPlaced, EnemyHQPlaced, MissionsPlaced.
	 * @note Why: Stores connection degree per anchor for degree-based filters.
	 * @note Technical: Cached via CacheAnchorConnectionDegrees and reused by selection logic.
	 * @note Notes: Hard constraint when Min/Max connection degree rules are enabled.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, int32> AnchorConnectionDegreesByAnchorKey;

	/**
	 * @note Used in: ConnectionsCreated debug/analysis.
	 * @note Why: Preserves chokepoint scoring so designers can inspect connectivity bottlenecks.
	 * @note Technical: Derived data keyed by anchor GUID, typically computed after connections exist.
	 * @note Notes: Informational/diagnostic; does not directly gate placement.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, float> ChokepointScoresByAnchorKey;

	/**
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Tracks how many enemy items of each type have been placed.
	 * @note Technical: Updated as items are spawned; compared against required counts.
	 * @note Notes: Hard constraint; when counts are met, additional placement is skipped.
	 * @note Used in: EnemyWallPlaced.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<EMapEnemyItem, int32> EnemyItemPlacedCounts;

	/**
	 * @note Used in: NeutralObjectsPlaced.
	 * @note Why: Tracks how many neutral items of each type have been placed.
	 * @note Technical: Updated during neutral placement and used to stop once required counts are reached.
	 * @note Notes: Hard constraint; affects retry logic if counts cannot be satisfied.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<EMapNeutralObjectType, int32> NeutralItemPlacedCounts;

	/**
	 * @note Used in: MissionsPlaced.
	 * @note Why: Tracks how many missions of each type have been placed.
	 * @note Technical: Updated during mission placement and checked against required mission counts.
	 * @note Notes: Hard constraint; unmet counts can trigger backtracking.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<EMapMission, int32> MissionPlacedCounts;
};

/**
 * @brief Owns the plain-data contract between AGeneratorWorldCampaign and the worker solver.
 * @note Nothing in this namespace may require live UObjects on the worker thread.
 */
namespace WorldCampaignAsyncPlacement
{
	/**
	 * @brief Thread-safe copy of player HQ placement settings.
	 * @note Indices preserve actor identity when duplicate anchor keys exist.
	 */
	struct FPlayerHQPlacementRulesSnapshot
	{
		TArray<FGuid> AnchorCandidateKeys;
		TArray<int32> AnchorCandidateIndices;
		int32 MinAnchorDegreeForHQ = 2;
		int32 MinAnchorsWithinHops = 6;
		int32 MinAnchorsWithinHopsRange = 2;
		int32 SafeZoneMaxHops = 1;
	};

	/**
	 * @brief Thread-safe copy of enemy HQ placement settings.
	 * @note Indices preserve explicit candidate array order from the game thread.
	 */
	struct FEnemyHQPlacementRulesSnapshot
	{
		TArray<FGuid> AnchorCandidateKeys;
		TArray<int32> AnchorCandidateIndices;
		int32 MinAnchorDegree = 1;
		int32 MaxAnchorDegree = 3;
		ETopologySearchStrategy AnchorDegreePreference = ETopologySearchStrategy::NotSet;
	};

	/**
	 * @brief Thread-safe copy of enemy wall placement settings.
	 * @note Candidate indices let the worker mirror pointer-array ordering without touching actors.
	 */
	struct FEnemyWallPlacementRulesSnapshot
	{
		TArray<FGuid> AnchorCandidateKeys;
		TArray<int32> AnchorCandidateIndices;
		EEnemyTopologySearchStrategy Preference = EEnemyTopologySearchStrategy::None;
	};

	/**
	 * @brief Thread-safe copy of mission-specific placement overrides.
	 * @note Override indices preserve actor identity for mission arrays with duplicate anchor keys.
	 */
	struct FPerMissionRulesSnapshot
	{
		EMissionTier Tier = EMissionTier::NotSet;
		bool bOverrideTierRules = false;
		bool bOverridePlacementWithArray = false;
		TArray<FGuid> OverridePlacementAnchorCandidateKeys;
		TArray<int32> OverridePlacementAnchorCandidateIndices;
		int32 OverrideMinConnections = 0;
		int32 OverrideMaxConnections = TNumericLimits<int32>::Max();
		ETopologySearchStrategy OverrideConnectionPreference = ETopologySearchStrategy::NotSet;
		int32 OverrideMinHopsFromPlayerHQ = 0;
		int32 OverrideMaxHopsFromPlayerHQ = TNumericLimits<int32>::Max();
		ETopologySearchStrategy OverrideHopsPreference = ETopologySearchStrategy::NotSet;
		FMissionTierRules OverrideRules;
	};

	/**
	 * @brief Thread-safe copy of all mission placement rules.
	 */
	struct FMissionPlacementSnapshot
	{
		int32 M_HopsPreferenceStrength = 3;
		TMap<EMissionTier, FMissionTierRules> RulesByTier;
		TMap<EMapMission, FPerMissionRulesSnapshot> RulesByMission;
	};

	/**
	 * @brief Immutable anchor data used for worker-side graph traversal.
	 * @note Neighbor indices are authoritative for graph identity; keys remain output identifiers.
	 */
	struct FAnchorSnapshot
	{
		FGuid AnchorKey;
		FVector Location = FVector::ZeroVector;
		TArray<FGuid> NeighborAnchorKeys;
		TArray<int32> NeighborAnchorIndices;
		int32 ConnectionDegree = 0;
		bool bIsEnemyWallCandidate = false;
		bool bIsPlayerHQCandidate = false;
		bool bIsEnemyHQCandidate = false;
		TSet<EMapMission> MissionOverrideCandidateMembership;
	};

	/**
	 * @brief Immutable connection data captured before worker placement starts.
	 */
	struct FConnectionSnapshot
	{
		TArray<FGuid> ConnectedAnchorKeys;
		bool bIsThreeWayConnection = false;
		FVector JunctionLocation = FVector::ZeroVector;
	};

	/**
	 * @brief Complete worker input captured after anchors and connections are generated.
	 * @note This is the determinism boundary; the worker must not read generator state directly.
	 */
	struct FPlacementSnapshot
	{
		int32 SeedUsed = 0;
		int32 InitialTotalAttemptCount = 0;
		TArray<FAnchorSnapshot> Anchors;
		TArray<int32> AnchorIndicesInCachedOrder;
		TArray<FConnectionSnapshot> Connections;
		TMap<ECampaignGenerationStep, int32> InitialStepAttemptIndices;
		FWorldCampaignDerivedData InitialDerivedData;
		FPlayerHQPlacementRulesSnapshot PlayerHQPlacementRules;
		FEnemyHQPlacementRulesSnapshot EnemyHQPlacementRules;
		FEnemyWallPlacementRulesSnapshot EnemyWallPlacementRules;
		FEnemyPlacementRules EnemyPlacementRules;
		FNeutralItemPlacementRules NeutralItemPlacementRules;
		FMissionPlacementSnapshot MissionPlacementRules;
		TArray<EMapMission> ExcludedMissionsFromMapPlacement;
		FWorldCampaignCountDifficultyTuning CountAndDifficultyTuning;
		FWorldCampaignPlacementFailurePolicy PlacementFailurePolicy;
	};

	/**
	 * @brief Worker diagnostic event deferred for game-thread reporting.
	 */
	struct FPlacementDebugEvent
	{
		ECampaignGenerationStep Step = ECampaignGenerationStep::NotStarted;
		FString Message;
	};

	/**
	 * @brief Complete worker result that the game thread can realize into actors.
	 * @note Contains only keys, counts, derived data, and diagnostics; no UObject references.
	 */
	struct FPlacementResult
	{
		bool bSucceeded = false;
		bool bRequiresGameThreadBacktrack = false;
		FString FailureReason;
		ECampaignGenerationStep GameThreadBacktrackFailedStep = ECampaignGenerationStep::NotStarted;
		int32 GameThreadTransactionsToUndo = 0;
		int32 WorkerTotalAttemptCount = 0;
		TMap<ECampaignGenerationStep, int32> StepAttemptIndicesAtEnd;
		FGuid PlayerHQAnchorKey;
		FGuid EnemyHQAnchorKey;
		TMap<FGuid, EMapEnemyItem> EnemyItemsByAnchorKey;
		TMap<FGuid, EMapNeutralObjectType> NeutralItemsByAnchorKey;
		TMap<FGuid, EMapMission> MissionsByAnchorKey;
		FWorldCampaignDerivedData DerivedData;
		TArray<FPlacementDebugEvent> DebugEvents;
	};

	/**
	 * @brief Shared progress snapshot read by the game thread while the worker is running.
	 * @note The critical section guards all fields because the worker writes them from another thread.
	 */
	struct FPlacementProgress
	{
		FCriticalSection CriticalSection;
		FString Phase;
		ECampaignGenerationStep CurrentStep = ECampaignGenerationStep::NotStarted;
		ECampaignGenerationStep LastFailedStep = ECampaignGenerationStep::NotStarted;
		int32 TotalAttemptCount = 0;
		int32 WorkerAttemptDelta = 0;
		int32 CurrentStepAttemptCount = 0;
		int32 TransactionCount = 0;
		int32 EnemyMicroPlacedCount = 0;
		int32 MissionMicroPlacedCount = 0;
		int32 NeutralTypeBeingEvaluated = 0;
		int32 NeutralEvaluatedAnchorCount = 0;
		int32 NeutralCandidateCount = 0;
		int32 NeutralRejectedOccupiedCount = 0;
		int32 NeutralRejectedNoHopCount = 0;
		int32 NeutralRejectedHopBandCount = 0;
		int32 NeutralRejectedSpacingCount = 0;
	};

	/**
	 * @brief Runs placement backtracking on immutable snapshot data away from live world actors.
	 * @param Snapshot Complete plain-data graph and rule snapshot captured on the game thread.
	 * @param Progress Optional thread-safe progress state for watchdog logging while solving.
	 * @return Pure placement result that the game thread can apply or discard.
	 */
	FPlacementResult SolvePlacement(
		const FPlacementSnapshot& Snapshot,
		const TSharedPtr<FPlacementProgress, ESPMode::ThreadSafe>& Progress = nullptr);
}
