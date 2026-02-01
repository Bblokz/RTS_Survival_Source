#pragma once

#include "CoreMinimal.h"

#include "PlayerHQPlacementRules.generated.h"

class AAnchorPoint;

/**
 * @brief Player HQ placement rules applied during PlayerHQPlaced, which follows
 *        ConnectionsCreated and precedes EnemyHQPlaced in the campaign generation flow.
 */
USTRUCT(BlueprintType)
struct FPlayerHQPlacementRules
{
	GENERATED_BODY()

	/**
	 * Candidate anchors allowed for player HQ placement.
	 * Example: Provide a curated list of start anchors to enforce a specific campaign opening.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	TArray<TObjectPtr<AAnchorPoint>> AnchorCandidates;

	/**
	 * Minimum anchor connection degree allowed for the player HQ.
	 * Example: Increase this to avoid placing the HQ on dead-ends so the early route has options.
	 */
	// Anchor degree constraints: avoid dead-ends (typical 2 or 3)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinAnchorDegreeForHQ = 2;

	/**
	 * Minimum count of anchors required within MinAnchorsWithinHopsRange.
	 * Example: Raise this to ensure the player starts in a dense neighborhood for early branching.
	 */
	// Start neighborhood size: ensure enough anchors nearby to populate early content.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinAnchorsWithinHops = 6;

	/**
	 * Hop range used to evaluate MinAnchorsWithinHops around the HQ.
	 * Example: Increase this to allow a wider starting neighborhood when the map is sparse.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinAnchorsWithinHopsRange = 2;

	/**
	 * Hop radius around the HQ that must be free of enemy objects.
	 * Example: Increase this to protect the player from immediate enemy contact in early campaigns.
	 */
	// Safe ring: no enemies inside a hop radius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 SafeZoneMaxHops = 1;
};
