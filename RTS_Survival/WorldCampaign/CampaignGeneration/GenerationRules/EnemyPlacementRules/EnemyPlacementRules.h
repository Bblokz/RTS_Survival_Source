#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/EnemyRuleVariantSelectionMode/Enum_EnemyRuleVariantSelectionMode.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/EnemyTopologySearchStrategy/Enum_EnemyTopologySearchStrategy.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"

#include "EnemyPlacementRules.generated.h"

/**
 * @brief Spacing rules between enemy items during EnemyObjectsPlaced, which happens after
 *        EnemyHQPlaced and before NeutralObjectsPlaced in campaign generation.
 */
USTRUCT(BlueprintType)
struct FEnemyItemToItemSpacingRules
{
	GENERATED_BODY()

	/**
	 * Minimum hop distance between different enemy item types.
	 * Example: Increase this to keep factories and airfields from clustering so the player
	 * must travel farther between distinct enemy facilities.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	int32 MinEnemySeparationHopsOtherType = 0;

	/**
	 * Minimum hop distance between enemy items of the same type.
	 * Example: Raise this to spread out repeated barracks placements and reduce stacked
	 * defenses in a single region.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	int32 MinEnemySeparationHopsSameType = 0;
};

/**
 * @brief Distance rules between enemy items and the enemy HQ during EnemyObjectsPlaced,
 *        which follows EnemyHQPlaced and precedes NeutralObjectsPlaced.
 */
USTRUCT(BlueprintType)
struct FEnemyItemToEnemyHQSpacingRules
{
	GENERATED_BODY()

	/**
	 * Minimum hop distance from the enemy HQ for this item.
	 * Example: Increase this to keep fragile support buildings away from the HQ so the
	 * player must push deeper before encountering them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	int32 MinHopsFromEnemyHQ = 0;

	/**
	 * Maximum hop distance from the enemy HQ for this item.
	 * Example: Lower this to keep high-value targets within a compact defensive ring.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	int32 MaxHopsFromEnemyHQ = 0;

	/**
	 * Preference for biasing placement within the Min/Max hop bounds.
	 * Example: PreferNearMaxBound to push an enemy outpost toward the edge of the enemy network.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	EEnemyTopologySearchStrategy Preference = EEnemyTopologySearchStrategy::None;
};

/**
 * @brief Aggregates spacing rules for a single enemy item type during EnemyObjectsPlaced,
 *        which occurs after EnemyHQPlaced and before NeutralObjectsPlaced.
 */
USTRUCT(BlueprintType)
struct FEnemyItemPlacementRules
{
	GENERATED_BODY()

	/**
	 * Spacing rules against other enemy items.
	 * Example: Tighten these when you want dense enemy clusters near chokepoints.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	FEnemyItemToItemSpacingRules ItemSpacing;

	/**
	 * Spacing rules relative to the enemy HQ.
	 * Example: Widen these to ensure certain enemy items appear as outer-ring objectives.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	FEnemyItemToEnemyHQSpacingRules EnemyHQSpacing;
};

/**
 * @brief Optional variant for enemy item placement rules during EnemyObjectsPlaced,
 *        used after EnemyHQPlaced and before NeutralObjectsPlaced to diversify layouts.
 */
USTRUCT(BlueprintType)
struct FEnemyItemPlacementVariant
{
	GENERATED_BODY()

	/**
	 * When false, this variant is skipped during variant selection.
	 * Example: Disable a variant temporarily when you are balancing a specific mission path.
	 */
	// When false, ignore this variant entirely.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant")
	bool bEnabled = true;

	/**
	 * When true, use OverrideRules instead of the base rules for this placement.
	 * Example: Enable this to create a rare layout where a factory is allowed closer to the HQ.
	 */
	// If true, use these rules instead of base for this placement.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant")
	bool bOverrideRules = false;

	/**
	 * The rules to use when this variant overrides the base rules.
	 * Example: Set a shorter MinEnemySeparationHopsSameType to permit a clustered defense pair.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant", meta = (EditCondition = "bOverrideRules"))
	FEnemyItemPlacementRules OverrideRules;
};

/**
 * @brief Ruleset for an enemy item type during EnemyObjectsPlaced, which runs after
 *        EnemyHQPlaced and before NeutralObjectsPlaced in the generation flow.
 */
USTRUCT(BlueprintType)
struct FEnemyItemRuleset
{
	GENERATED_BODY()

	/**
	 * Baseline rules that apply when no variant overrides are selected.
	 * Example: Establish a conservative spacing baseline, then use variants to loosen it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ruleset")
	FEnemyItemPlacementRules BaseRules;

	/**
	 * How to select variants when multiple placements of this enemy item occur.
	 * Example: Use CycleByPlacementIndex to repeat a predictable pattern along a chain of anchors.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ruleset")
	EEnemyRuleVariantSelectionMode VariantMode = EEnemyRuleVariantSelectionMode::None;

	/**
	 * Optional placement variants used based on VariantMode.
	 * Example: Add a variant with longer MinHopsFromEnemyHQ to create a late-campaign outpost.
	 */
	// Example: 3 variants -> placements 0,3,6 use Variants[0], 1,4,7 use [1], 2,5,8 use [2].
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ruleset", meta = (EditCondition = "VariantMode!=EEnemyRuleVariantSelectionMode::None"))
	TArray<FEnemyItemPlacementVariant> Variants;
};

/**
 * @brief Root rule map for all enemy item placements during EnemyObjectsPlaced, occurring
 *        after EnemyHQPlaced and before NeutralObjectsPlaced in campaign generation.
 */
USTRUCT(BlueprintType)
struct FEnemyPlacementRules
{
	GENERATED_BODY()

	/**
	 * Per-enemy-item rulesets used to place each enemy item type.
	 * Example: Increase spacing for EnemyHQ-adjacent factories without affecting outposts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	TMap<EMapEnemyItem, FEnemyItemRuleset> RulesByEnemyItem;
};
