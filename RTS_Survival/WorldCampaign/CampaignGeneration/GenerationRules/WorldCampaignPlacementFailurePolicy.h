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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy GlobalPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the ConnectionsCreated step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack to loosen topology constraints before retrying.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy CreateConnectionsPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the PlayerHQPlaced step fails.
	 * Example: Use InstantBackTrack to rebuild connections if no valid HQ anchors exist.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceHQPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the EnemyHQPlaced step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack to allow a wider HQ selection pool.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceEnemyHQPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the EnemyObjectsPlaced step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack to relax spacing if enemy clusters are too strict.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceEnemyObjectsPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the NeutralObjectsPlaced step fails.
	 * Example: Use BreakDistanceRules_ThenBackTrack if neutral item spacing blocks placement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceNeutralObjectsPolicy = EPlacementFailurePolicy::NotSet;

	/**
	 * Policy used when the MissionsPlaced step fails.
	 * Example: Use InstantBackTrack to retry mission placement without altering earlier steps.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	EPlacementFailurePolicy PlaceMissionsPolicy = EPlacementFailurePolicy::NotSet;
};
