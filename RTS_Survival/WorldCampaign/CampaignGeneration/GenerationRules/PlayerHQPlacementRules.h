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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = "Used in: PlayerHQPlaced -> ExecutePlaceHQ/BuildHQAnchorCandidates.\n"
		                  "Why: Restricts the HQ to specific anchors for authored starts or curated openings.\n"
		                  "Technical: Acts as a hard filter before any degree or hop checks are applied.\n"
		                  "Notes: Empty means all anchors are candidates; non-empty hard-gates selection."))
	TArray<TObjectPtr<AAnchorPoint>> AnchorCandidates;

	/**
	 * Minimum anchor connection degree allowed for the player HQ.
	 * Example: Increase this to avoid placing the HQ on dead-ends so the early route has options.
	 */
	// Anchor degree constraints: avoid dead-ends (typical 2 or 3)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = "Used in: PlayerHQPlaced -> BuildHQAnchorCandidates.\n"
		                  "Why: Prevents the player HQ from spawning on low-connectivity dead-ends.\n"
		                  "Technical: Hard minimum on cached connection degree per anchor.\n"
		                  "Notes: If set too high relative to the map, HQ placement will fail and backtrack."))
	int32 MinAnchorDegreeForHQ = 2;

	/**
	 * Minimum count of anchors required within MinAnchorsWithinHopsRange.
	 * Example: Raise this to ensure the player starts in a dense neighborhood for early branching.
	 */
	// Start neighborhood size: ensure enough anchors nearby to populate early content.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = "Used in: PlayerHQPlaced.\n"
		                  "Why: Ensures the player starts near enough anchors to seed early objectives.\n"
		                  "Technical: Hard filter requiring at least this many anchors within MinAnchorsWithinHopsRange.\n"
		                  "Notes: If too high, valid HQ anchors may be eliminated entirely."))
	int32 MinAnchorsWithinHops = 6;

	/**
	 * Hop range used to evaluate MinAnchorsWithinHops around the HQ.
	 * Example: Increase this to allow a wider starting neighborhood when the map is sparse.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = "Used in: PlayerHQPlaced.\n"
		                  "Why: Defines the hop radius for the start-neighborhood count check.\n"
		                  "Technical: Drives the hop-distance query against cached connection distances.\n"
		                  "Notes: Must be >= 0; larger values make the neighborhood check easier to satisfy."))
	int32 MinAnchorsWithinHopsRange = 2;

	/**
	 * Hop radius around the HQ that must be free of enemy objects.
	 * Example: Increase this to protect the player from immediate enemy contact in early campaigns.
	 */
	// Safe ring: no enemies inside a hop radius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced and validation against PlayerHQPlaced.\n"
		                  "Why: Reserves a safety buffer so early combat does not spawn adjacent to the HQ.\n"
		                  "Technical: Treated as a hard exclusion radius in hop distance from PlayerHQAnchor.\n"
		                  "Notes: If too large, it can starve enemy placement and trigger backtracking."))
	int32 SafeZoneMaxHops = 1;
};
