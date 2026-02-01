#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/TopologySearchStrategy/Enum_TopologySearchStrategy.h"

#include "EnemyHQPlacementRules.generated.h"

class AAnchorPoint;

/**
 * @brief Defines how the enemy HQ is anchored during the EnemyHQPlaced step, which happens
 *        after PlayerHQPlaced and before EnemyObjectsPlaced in campaign generation.
 */
USTRUCT(BlueprintType)
struct FEnemyHQPlacementRules
{
	GENERATED_BODY()

	/**
	 * Candidate anchors allowed for the enemy HQ during the EnemyHQPlaced step.
	 * Example: Restrict this list to anchors on the far side of the map to keep early
	 * encounters away from the player HQ while still allowing later enemy object placement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	TArray<TObjectPtr<AAnchorPoint>> AnchorCandidates;

	/**
	 * Minimum connection degree for an anchor to be eligible for the enemy HQ.
	 * Example: Increase this if you want the enemy HQ to sit on a more connected hub
	 * so that later EnemyObjectsPlaced can branch outward easily.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinAnchorDegree = 1;

	/**
	 * Maximum connection degree for an anchor to be eligible for the enemy HQ.
	 * Example: Lower this if you want the enemy HQ to be tucked into a smaller branch
	 * so the player has fewer approach paths before NeutralObjectsPlaced.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MaxAnchorDegree = 3;

	/**
	 * Preference for how to bias candidate selection within the Min/Max degree bounds.
	 * Example: PreferMax to push the enemy HQ toward highly connected nodes when you want
	 * the enemy object network to expand quickly in subsequent steps.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	ETopologySearchStrategy AnchorDegreePreference = ETopologySearchStrategy::NotSet;
};
