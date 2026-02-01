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
	 * Global fallback policy when a step-specific policy is not set.
	 * Example: Use InstantBackTrack to immediately roll back failed steps during iteration.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = "Used in: HandleStepFailure when a step-specific policy is NotSet.\n"
		                  "Why: Provides a single fallback policy for all steps to simplify tuning.\n"
		                  "Technical: Read by GetFailurePolicyForStep; acts as the default.\n"
		                  "Notes: Determines whether backtracking or rule loosening is attempted."))
	EPlacementFailurePolicy GlobalPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the ConnectionsCreated step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack to loosen topology constraints before retrying.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = "Used in: HandleStepFailure for ConnectionsCreated.\n"
		                  "Why: Controls how the generator reacts when connection creation fails.\n"
		                  "Technical: Can trigger rule loosening or immediate backtracking of connection steps.\n"
		                  "Notes: Influences determinism because it changes how attempts are sequenced."))
	EPlacementFailurePolicy CreateConnectionsPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the PlayerHQPlaced step fails.
	 * Example: Use InstantBackTrack to rebuild connections if no valid HQ anchors exist.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = "Used in: HandleStepFailure for PlayerHQPlaced.\n"
		                  "Why: Controls whether the generator retries HQ placement or backtracks earlier steps.\n"
		                  "Technical: Applied when ExecutePlaceHQ fails to find candidates.\n"
		                  "Notes: Selecting a backtrack policy increases total attempts deterministically."))
	EPlacementFailurePolicy PlaceHQPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the EnemyHQPlaced step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack to allow a wider HQ selection pool.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = "Used in: HandleStepFailure for EnemyHQPlaced.\n"
		                  "Why: Controls retry/backtrack behavior when enemy HQ placement cannot satisfy rules.\n"
		                  "Technical: Determines whether to relax rules or undo previous steps.\n"
		                  "Notes: Affects deterministic retry ordering and attempt counts."))
	EPlacementFailurePolicy PlaceEnemyHQPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the EnemyObjectsPlaced step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack to relax spacing if enemy clusters are too strict.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = "Used in: HandleStepFailure for EnemyObjectsPlaced.\n"
		                  "Why: Controls how to respond when enemy placement cannot meet spacing/count constraints.\n"
		                  "Technical: Determines whether rules are relaxed or earlier steps are rolled back.\n"
		                  "Notes: Influences determinism by changing the order and number of retries."))
	EPlacementFailurePolicy PlaceEnemyObjectsPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the NeutralObjectsPlaced step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack if neutral item spacing blocks placement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = "Used in: HandleStepFailure for NeutralObjectsPlaced.\n"
		                  "Why: Controls retry/backtrack behavior when neutral item placement cannot satisfy rules.\n"
		                  "Technical: Decides whether to relax spacing or undo prior steps.\n"
		                  "Notes: Backtracking increases total attempts and changes deterministic paths."))
	EPlacementFailurePolicy PlaceNeutralObjectsPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the MissionsPlaced step fails.
	 * Example: Use InstantBackTrack to retry mission placement without altering earlier steps.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = "Used in: HandleStepFailure for MissionsPlaced.\n"
		                  "Why: Controls how mission placement failures are handled.\n"
		                  "Technical: Determines whether to retry mission placement or undo earlier steps.\n"
		                  "Notes: Impacts deterministic retry order and total attempt count."))
	EPlacementFailurePolicy PlaceMissionsPolicy = EPlacementFailurePolicy::NotSet;
};
