#pragma once

#include "CoreMinimal.h"
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
