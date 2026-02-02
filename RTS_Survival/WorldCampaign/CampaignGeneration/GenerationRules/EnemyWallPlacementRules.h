#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/EnemyTopologySearchStrategy/Enum_EnemyTopologySearchStrategy.h"

#include "EnemyWallPlacementRules.generated.h"

class AAnchorPoint;

/**
 * @brief Placement rules for selecting a single enemy wall anchor during EnemyWallPlaced,
 *        which runs after EnemyHQPlaced and before EnemyObjectsPlaced.
 */
USTRUCT(BlueprintType)
struct FEnemyWallPlacementRules
{
	GENERATED_BODY()

	/**
	 * Designer-provided list of anchors that are eligible for the wall placement.
	 * @note Used in: EnemyWallPlaced.
	 * @note Why: Allows explicit authoring of valid wall anchors without searching the full graph.
	 * @note Technical: Filtered against cached anchors and occupancy before selection.
	 * @note Notes: If empty or invalid, the step fails so backtracking can respond.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Wall")
	TArray<TObjectPtr<AAnchorPoint>> AnchorCandidates;

	/**
	 * Preference for ordering candidates when multiple anchors are valid.
	 * @note Used in: EnemyWallPlaced candidate ordering.
	 * @note Why: Provides deterministic bias toward degrees or chokepoints.
	 * @note Technical: Applied as a soft score with stable anchor-key tie breaks.
	 * @note Notes: PreferNearMin/Max are treated as no preference for walls.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Wall")
	EEnemyTopologySearchStrategy Preference = EEnemyTopologySearchStrategy::None;
};
