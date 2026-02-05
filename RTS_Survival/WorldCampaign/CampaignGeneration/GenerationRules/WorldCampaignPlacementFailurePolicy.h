#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/PlacementFailurePolicy/Enum_PlacementFailurePolicy.h"

#include "WorldCampaignPlacementFailurePolicy.generated.h"

/**
 * @brief Failure policies for each generation step, used when a placement step fails
 *        in the sequence from ConnectionsCreated through MissionsPlaced.
 */
USTRUCT(BlueprintType)
struct FWorldCampaignPlacementFailurePolicy
{
	GENERATED_BODY()

	/**
	 * Micro-transaction escalation window. After this many failed micro-undo retries at the current depth,
	 * the generator escalates by also undoing one more previous micro transaction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy",
		meta = (ClampMin = "1", ClampMax = "1024",
			ToolTip = "Micro-transaction escalation window. After this many failed micro-undo retries at the current depth, the generator escalates by also undoing one more previous micro transaction."))
	int32 EscalationAttempts = 32;

	/**
	 * @note Used in: Timeout fail-safe placement when attempts exceed limits.
	 * @note Why: Enables a deterministic best-effort placement instead of hard-failing.
	 * @note Technical: Checked by HandleStepFailure before returning false.
	 * @note Notes: Disabling restores the previous hard-fail behavior.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy")
	bool bEnableTimeoutFailSafe = true;

	/**
	 * @note Used in: Timeout fail-safe placement fallback shuffle.
	 * @note Why: Adds a stable offset to the deterministic random stream seed.
	 * @note Technical: Combined with SeedUsed and the failed step to shuffle remaining anchors.
	 * @note Notes: Adjust only if you need different deterministic fallback ordering.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy",
		meta = (ClampMin = "0", ClampMax = "100000"))
	int32 TimeoutFailSafeSeedOffset = 5501;

	/**
	 * @note Used in: Timeout fail-safe placement.
	 * @note Why: Adds extra XY spacing between anchors selected for the same fail-safe item kind.
	 * @note Technical: Compared against squared XY distance for previously placed same-kind anchors.
	 * @note Notes: 0 disables this check to preserve previous behavior.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy",
		meta = (ClampMin = "0.0"))
	float TimeoutFailSafeMinSameKindXYSpacing = 0.f;

	/**
	 * @note Used in: Timeout fail-safe placement.
	 * @note Why: Sets minimum XY distance from Player HQ for Tier1 missions.
	 * @note Technical: Compared against squared XY distance in the fail-safe pass.
	 * @note Notes: Defaults to 0 to allow placement anywhere when unset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy")
	float MinDistancePlayerHQTier1Mission = 0.0f;

	/**
	 * @note Used in: Timeout fail-safe placement.
	 * @note Why: Sets minimum XY distance from Player HQ for Tier2 missions.
	 * @note Technical: Compared against squared XY distance in the fail-safe pass.
	 * @note Notes: Defaults to 0 to allow placement anywhere when unset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy")
	float MinDistancePlayerHQTier2Mission = 0.0f;

	/**
	 * @note Used in: Timeout fail-safe placement.
	 * @note Why: Sets minimum XY distance from Player HQ for Tier3 missions.
	 * @note Technical: Compared against squared XY distance in the fail-safe pass.
	 * @note Notes: Defaults to 0 to allow placement anywhere when unset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy")
	float MinDistancePlayerHQTier3Mission = 0.0f;

	/**
	 * @note Used in: Timeout fail-safe placement.
	 * @note Why: Sets minimum XY distance from Player HQ for Tier4 missions.
	 * @note Technical: Compared against squared XY distance in the fail-safe pass.
	 * @note Notes: Defaults to 0 to allow placement anywhere when unset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy")
	float MinDistancePlayerHQTier4Mission = 0.0f;

	/**
	 * @note Used in: Timeout fail-safe placement.
	 * @note Why: Per-enemy-type minimum XY distance from Player HQ.
	 * @note Technical: Missing keys default to 0.0f in FindRef.
	 * @note Notes: Use for last-resort spacing when the generator times out.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy")
	TMap<EMapEnemyItem, float> MinDistancePlayerHQEnemyItemByType;

	/**
	 * @note Used in: Timeout fail-safe placement.
	 * @note Why: Per-neutral-type minimum XY distance from Player HQ.
	 * @note Technical: Missing keys default to 0.0f in FindRef.
	 * @note Notes: Applies only to the fail-safe placement pass.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy")
	TMap<EMapNeutralObjectType, float> MinDistancePlayerHQNeutralByType;

	/**
	 * Global fallback policy when a step-specific policy is not set.
	 * Example: Use InstantBackTrack to immediately roll back failed steps during iteration.
	 * @note Used in: HandleStepFailure when a step-specific policy is NotSet.
	 * @note Why: Provides a single fallback policy for all steps to simplify tuning.
	 * @note Technical: Read by GetFailurePolicyForStep; acts as the default.
	 * @note Notes: Determines whether backtracking or rule loosening is attempted.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy GlobalPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the ConnectionsCreated step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack to loosen topology constraints before retrying.
	 * @note Used in: HandleStepFailure for ConnectionsCreated.
	 * @note Why: Controls how the generator reacts when connection creation fails.
	 * @note Technical: Can trigger rule loosening or immediate backtracking of connection steps.
	 * @note Notes: Influences determinism because it changes how attempts are sequenced.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy CreateConnectionsPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the PlayerHQPlaced step fails.
	 * Example: Use InstantBackTrack to rebuild connections if no valid HQ anchors exist.
	 * @note Used in: HandleStepFailure for PlayerHQPlaced.
	 * @note Why: Controls whether the generator retries HQ placement or backtracks earlier steps.
	 * @note Technical: Applied when ExecutePlaceHQ fails to find candidates.
	 * @note Notes: Selecting a backtrack policy increases total attempts deterministically.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceHQPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the EnemyHQPlaced step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack to allow a wider HQ selection pool.
	 * @note Used in: HandleStepFailure for EnemyHQPlaced.
	 * @note Why: Controls retry/backtrack behavior when enemy HQ placement cannot satisfy rules.
	 * @note Technical: Determines whether to relax rules or undo previous steps.
	 * @note Notes: Affects deterministic retry ordering and attempt counts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceEnemyHQPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the EnemyWallPlaced step fails.
	 * Example: Use InstantBackTrack to retry wall placement without widening the candidate list.
	 * @note Used in: HandleStepFailure for EnemyWallPlaced.
	 * @note Why: Controls retry/backtrack behavior for the wall placement step.
	 * @note Technical: Determines whether to retry the step or undo prior steps.
	 * @note Notes: Affects determinism by altering retry ordering.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceEnemyWallPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the EnemyObjectsPlaced step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack to relax spacing if enemy clusters are too strict.
	 * @note Used in: HandleStepFailure for EnemyObjectsPlaced.
	 * @note Why: Controls how to respond when enemy placement cannot meet spacing/count constraints.
	 * @note Technical: Determines whether rules are relaxed or earlier steps are rolled back.
	 * @note Notes: Influences determinism by changing the order and number of retries.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceEnemyObjectsPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the NeutralObjectsPlaced step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack if neutral item spacing blocks placement.
	 * @note Used in: HandleStepFailure for NeutralObjectsPlaced.
	 * @note Why: Controls retry/backtrack behavior when neutral item placement cannot satisfy rules.
	 * @note Technical: Decides whether to relax spacing or undo prior steps.
	 * @note Notes: Backtracking increases total attempts and changes deterministic paths.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceNeutralObjectsPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the MissionsPlaced step fails.
	 * Example: Use InstantBackTrack to retry mission placement without altering earlier steps.
	 * @note Used in: HandleStepFailure for MissionsPlaced.
	 * @note Why: Controls how mission placement failures are handled.
	 * @note Technical: Determines whether to retry mission placement or undo earlier steps.
	 * @note Notes: Impacts deterministic retry order and total attempt count.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceMissionsPolicy = EPlacementFailurePolicy::NotSet;
};
