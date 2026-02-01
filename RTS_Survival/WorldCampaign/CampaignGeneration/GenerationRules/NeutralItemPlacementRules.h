#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/TopologySearchStrategy/Enum_TopologySearchStrategy.h"

#include "NeutralItemPlacementRules.generated.h"

/**
 * @brief Rules for neutral item placement during NeutralObjectsPlaced, which runs after
 *        EnemyObjectsPlaced and before MissionsPlaced in campaign generation.
 */
USTRUCT(BlueprintType)
struct FNeutralItemPlacementRules
{
	GENERATED_BODY()

	/**
	 * Minimum hop distance from the player HQ for neutral items.
	 * Example: Increase this to keep neutral resources out of the player’s immediate start area.
	 * @note Used in: NeutralObjectsPlaced.
	 * @note Why: Keeps neutral items out of the immediate player HQ neighborhood.
	 * @note Technical: Hard minimum hop distance filter from PlayerHQAnchor.
	 * @note Notes: Must be <= MaxHopsFromHQ; too high can eliminate all candidates.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinHopsFromHQ = 0;

	/**
	 * Maximum hop distance from the player HQ for neutral items.
	 * Example: Lower this if you want neutral items concentrated near mid-map for early access.
	 * @note Used in: NeutralObjectsPlaced.
	 * @note Why: Keeps neutral items within a bounded travel radius from the player HQ.
	 * @note Technical: Hard maximum hop distance filter from PlayerHQAnchor.
	 * @note Notes: Must be >= MinHopsFromHQ; too low can starve placements.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MaxHopsFromHQ = 0;

	/**
	 * Minimum hop distance between neutral items.
	 * Example: Increase this to reduce clustering of resource nodes and encourage exploration.
	 * @note Used in: NeutralObjectsPlaced.
	 * @note Why: Enforces a minimum hop spacing between neutral items to prevent clustering.
	 * @note Technical: Hard filter against cached neutral placements by hop distance.
	 * @note Notes: Must be <= MaxHopsFromOtherNeutralItems; too high may block placement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinHopsFromOtherNeutralItems = 0;

	/**
	 * Maximum hop distance between neutral items.
	 * Example: Lower this to allow multiple neutral items to appear within the same region.
	 * @note Used in: NeutralObjectsPlaced.
	 * @note Why: Caps spacing so neutral items can cluster if desired.
	 * @note Technical: Hard maximum hop distance filter relative to other neutral items.
	 * @note Notes: Must be >= MinHopsFromOtherNeutralItems; too low can force tight clustering.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MaxHopsFromOtherNeutralItems = 0;

	/**
	 * Preference for biasing placement within the hop bounds.
	 * Example: PreferMin to keep neutral items closer to the player HQ after enemy objects are placed.
	 * @note Used in: NeutralObjectsPlaced candidate ordering.
	 * @note Why: Lets designers bias where within the Min/Max hop window neutral items land.
	 * @note Technical: Soft preference applied during candidate sorting; does not bypass hard bounds.
	 * @note Notes: Deterministic ordering also uses anchor keys and attempt index.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	ETopologySearchStrategy Preference = ETopologySearchStrategy::NotSet;
};
