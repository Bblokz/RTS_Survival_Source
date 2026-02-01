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
		meta = (ToolTip = R"(Used in: HandleStepFailure when a step-specific policy is NotSet.
Why: Provides a single fallback policy for all steps to simplify tuning.
Technical: Read by GetFailurePolicyForStep; acts as the default.
Notes: Determines whether backtracking or rule loosening is attempted.)"))
	EPlacementFailurePolicy GlobalPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the ConnectionsCreated step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack to loosen topology constraints before retrying.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = R"(Used in: HandleStepFailure for ConnectionsCreated.
Why: Controls how the generator reacts when connection creation fails.
Technical: Can trigger rule loosening or immediate backtracking of connection steps.
Notes: Influences determinism because it changes how attempts are sequenced.)"))
	EPlacementFailurePolicy CreateConnectionsPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the PlayerHQPlaced step fails.
	 * Example: Use InstantBackTrack to rebuild connections if no valid HQ anchors exist.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = R"(Used in: HandleStepFailure for PlayerHQPlaced.
Why: Controls whether the generator retries HQ placement or backtracks earlier steps.
Technical: Applied when ExecutePlaceHQ fails to find candidates.
Notes: Selecting a backtrack policy increases total attempts deterministically.)"))
	EPlacementFailurePolicy PlaceHQPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the EnemyHQPlaced step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack to allow a wider HQ selection pool.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = R"(Used in: HandleStepFailure for EnemyHQPlaced.
Why: Controls retry/backtrack behavior when enemy HQ placement cannot satisfy rules.
Technical: Determines whether to relax rules or undo previous steps.
Notes: Affects deterministic retry ordering and attempt counts.)"))
	EPlacementFailurePolicy PlaceEnemyHQPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the EnemyObjectsPlaced step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack to relax spacing if enemy clusters are too strict.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = R"(Used in: HandleStepFailure for EnemyObjectsPlaced.
Why: Controls how to respond when enemy placement cannot meet spacing/count constraints.
Technical: Determines whether rules are relaxed or earlier steps are rolled back.
Notes: Influences determinism by changing the order and number of retries.)"))
	EPlacementFailurePolicy PlaceEnemyObjectsPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the NeutralObjectsPlaced step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack if neutral item spacing blocks placement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = R"(Used in: HandleStepFailure for NeutralObjectsPlaced.
Why: Controls retry/backtrack behavior when neutral item placement cannot satisfy rules.
Technical: Decides whether to relax spacing or undo prior steps.
Notes: Backtracking increases total attempts and changes deterministic paths.)"))
	EPlacementFailurePolicy PlaceNeutralObjectsPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the MissionsPlaced step fails.
	 * Example: Use InstantBackTrack to retry mission placement without altering earlier steps.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy",
		meta = (ToolTip = R"(Used in: HandleStepFailure for MissionsPlaced.
Why: Controls how mission placement failures are handled.
Technical: Determines whether to retry mission placement or undo earlier steps.
Notes: Impacts deterministic retry order and total attempt count.)"))
	EPlacementFailurePolicy PlaceMissionsPolicy = EPlacementFailurePolicy::NotSet;
};
