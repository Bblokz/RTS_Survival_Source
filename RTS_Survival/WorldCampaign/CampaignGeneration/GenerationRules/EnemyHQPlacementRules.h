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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = "Used in: EnemyHQPlaced -> ExecutePlaceEnemyHQ/BuildHQAnchorCandidates.\n"
		                  "Why: Allows authored control over where the enemy HQ is allowed to appear.\n"
		                  "Technical: Acts as a hard filter before degree and hop constraints are evaluated.\n"
		                  "Notes: Empty means all anchors are candidates; non-empty strictly gates selection."))
	TArray<TObjectPtr<AAnchorPoint>> AnchorCandidates;

	/**
	 * Minimum connection degree for an anchor to be eligible for the enemy HQ.
	 * Example: Increase this if you want the enemy HQ to sit on a more connected hub
	 * so that later EnemyObjectsPlaced can branch outward easily.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = "Used in: EnemyHQPlaced -> BuildHQAnchorCandidates.\n"
		                  "Why: Ensures the enemy HQ sits on a sufficiently connected node.\n"
		                  "Technical: Hard minimum on cached connection degree for candidate anchors.\n"
		                  "Notes: Must be <= MaxAnchorDegree; too high can remove all candidates."))
	int32 MinAnchorDegree = 1;

	/**
	 * Maximum connection degree for an anchor to be eligible for the enemy HQ.
	 * Example: Lower this if you want the enemy HQ to be tucked into a smaller branch
	 * so the player has fewer approach paths before NeutralObjectsPlaced.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = "Used in: EnemyHQPlaced -> BuildHQAnchorCandidates.\n"
		                  "Why: Prevents the enemy HQ from spawning on overly connected hubs.\n"
		                  "Technical: Hard maximum on cached connection degree for candidate anchors.\n"
		                  "Notes: Must be >= MinAnchorDegree; too low can block placement entirely."))
	int32 MaxAnchorDegree = 3;

	/**
	 * Preference for how to bias candidate selection within the Min/Max degree bounds.
	 * Example: PreferMax to push the enemy HQ toward highly connected nodes when you want
	 * the enemy object network to expand quickly in subsequent steps.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = "Used in: EnemyHQPlaced candidate sorting/selection.\n"
		                  "Why: Allows designers to bias the selection toward low- or high-degree anchors.\n"
		                  "Technical: Soft preference used when ordering candidates; does not bypass Min/Max limits.\n"
		                  "Notes: Deterministic ordering uses this bias plus anchor keys and attempt index."))
	ETopologySearchStrategy AnchorDegreePreference = ETopologySearchStrategy::NotSet;
};
