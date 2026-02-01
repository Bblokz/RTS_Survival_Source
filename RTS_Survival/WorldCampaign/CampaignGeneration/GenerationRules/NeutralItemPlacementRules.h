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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinHopsFromHQ = 0;

	/**
	 * Maximum hop distance from the player HQ for neutral items.
	 * Example: Lower this if you want neutral items concentrated near mid-map for early access.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MaxHopsFromHQ = 0;

	/**
	 * Minimum hop distance between neutral items.
	 * Example: Increase this to reduce clustering of resource nodes and encourage exploration.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinHopsFromOtherNeutralItems = 0;

	/**
	 * Maximum hop distance between neutral items.
	 * Example: Lower this to allow multiple neutral items to appear within the same region.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MaxHopsFromOtherNeutralItems = 0;

	/**
	 * Preference for biasing placement within the hop bounds.
	 * Example: PreferMin to keep neutral items closer to the player HQ after enemy objects are placed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	ETopologySearchStrategy Preference = ETopologySearchStrategy::NotSet;
};
