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
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = R"(Used in: NeutralObjectsPlaced.
Why: Keeps neutral items out of the immediate player HQ neighborhood.
Technical: Hard minimum hop distance filter from PlayerHQAnchor.
Notes: Must be <= MaxHopsFromHQ; too high can eliminate all candidates.)"))
	int32 MinHopsFromHQ = 0;

	/**
	 * Maximum hop distance from the player HQ for neutral items.
	 * Example: Lower this if you want neutral items concentrated near mid-map for early access.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = R"(Used in: NeutralObjectsPlaced.
Why: Keeps neutral items within a bounded travel radius from the player HQ.
Technical: Hard maximum hop distance filter from PlayerHQAnchor.
Notes: Must be >= MinHopsFromHQ; too low can starve placements.)"))
	int32 MaxHopsFromHQ = 0;

	/**
	 * Minimum hop distance between neutral items.
	 * Example: Increase this to reduce clustering of resource nodes and encourage exploration.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = R"(Used in: NeutralObjectsPlaced.
Why: Enforces a minimum hop spacing between neutral items to prevent clustering.
Technical: Hard filter against cached neutral placements by hop distance.
Notes: Must be <= MaxHopsFromOtherNeutralItems; too high may block placement.)"))
	int32 MinHopsFromOtherNeutralItems = 0;

	/**
	 * Maximum hop distance between neutral items.
	 * Example: Lower this to allow multiple neutral items to appear within the same region.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = R"(Used in: NeutralObjectsPlaced.
Why: Caps spacing so neutral items can cluster if desired.
Technical: Hard maximum hop distance filter relative to other neutral items.
Notes: Must be >= MinHopsFromOtherNeutralItems; too low can force tight clustering.)"))
	int32 MaxHopsFromOtherNeutralItems = 0;

	/**
	 * Preference for biasing placement within the hop bounds.
	 * Example: PreferMin to keep neutral items closer to the player HQ after enemy objects are placed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = R"(Used in: NeutralObjectsPlaced candidate ordering.
Why: Lets designers bias where within the Min/Max hop window neutral items land.
Technical: Soft preference applied during candidate sorting; does not bypass hard bounds.
Notes: Deterministic ordering also uses anchor keys and attempt index.)"))
	ETopologySearchStrategy Preference = ETopologySearchStrategy::NotSet;
};
