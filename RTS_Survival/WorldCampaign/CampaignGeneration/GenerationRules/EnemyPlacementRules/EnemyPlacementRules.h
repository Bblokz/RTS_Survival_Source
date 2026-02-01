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
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Prevents different enemy item types from stacking too tightly.
	 * @note Technical: Hard minimum hop distance filter between different item types.
	 * @note Notes: Larger values reduce clustering but can cause placement failures.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	int32 MinEnemySeparationHopsOtherType = 0;

	/**
	 * Minimum hop distance between enemy items of the same type.
	 * Example: Raise this to spread out repeated barracks placements and reduce stacked
	 * defenses in a single region.
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Spreads identical enemy items so repeated defenses are not adjacent.
	 * @note Technical: Hard minimum hop distance filter between same-type items.
	 * @note Notes: High values reduce repetition but can starve placement candidates.
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
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Enforces a buffer around the enemy HQ for certain item types.
	 * @note Technical: Hard minimum hop distance from EnemyHQAnchor.
	 * @note Notes: Must be <= MaxHopsFromEnemyHQ; too high can remove valid anchors.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	int32 MinHopsFromEnemyHQ = 0;

	/**
	 * Maximum hop distance from the enemy HQ for this item.
	 * Example: Lower this to keep high-value targets within a compact defensive ring.
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Keeps certain enemy items within a bounded radius of the enemy HQ.
	 * @note Technical: Hard maximum hop distance from EnemyHQAnchor.
	 * @note Notes: Must be >= MinHopsFromEnemyHQ; too low can block placement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	int32 MaxHopsFromEnemyHQ = 0;

	/**
	 * Preference for biasing placement within the Min/Max hop bounds.
	 * Example: PreferNearMaxBound to push an enemy outpost toward the edge of the enemy network.
	 * @note Used in: EnemyObjectsPlaced candidate ordering.
	 * @note Why: Biases placements toward the near/min or far/max side of the hop window.
	 * @note Technical: Soft preference used in ordering; does not bypass hard hop bounds.
	 * @note Notes: Deterministic ordering also uses anchor keys and attempt index.
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
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Defines item-to-item spacing constraints for this enemy item type.
	 * @note Technical: Hard hop-distance filters applied against existing enemy placements.
	 * @note Notes: Violations cause candidate anchors to be rejected.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	FEnemyItemToItemSpacingRules ItemSpacing;

	/**
	 * Spacing rules relative to the enemy HQ.
	 * Example: Widen these to ensure certain enemy items appear as outer-ring objectives.
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Enforces min/max hop distance from the enemy HQ for this item type.
	 * @note Technical: Hard hop-distance filter based on EnemyHQHopDistancesByAnchorKey.
	 * @note Notes: If EnemyHQHopDistances is missing, this filter can fail placements.
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
	 * @note Used in: EnemyObjectsPlaced variant selection.
	 * @note Why: Allows designers to disable a variant without deleting it.
	 * @note Technical: Hard gate; disabled variants are skipped during selection.
	 * @note Notes: Deterministic selection ignores disabled entries entirely.
	 */
	// When false, ignore this variant entirely.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant")
	bool bEnabled = true;

	/**
	 * When true, use OverrideRules instead of the base rules for this placement.
	 * Example: Enable this to create a rare layout where a factory is allowed closer to the HQ.
	 * @note Used in: EnemyObjectsPlaced variant selection.
	 * @note Why: Lets this variant override the base rules for specific placements.
	 * @note Technical: When true, OverrideRules are applied as the hard filters for this placement.
	 * @note Notes: Drives the EditCondition for OverrideRules; disabling hides the override field.
	 */
	// If true, use these rules instead of base for this placement.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant")
	bool bOverrideRules = false;

	/**
	 * The rules to use when this variant overrides the base rules.
	 * Example: Set a shorter MinEnemySeparationHopsSameType to permit a clustered defense pair.
	 * @note Used in: EnemyObjectsPlaced when bOverrideRules is true.
	 * @note Why: Defines the variant-specific spacing rules for this placement.
	 * @note Technical: Replaces BaseRules for the selected placement index.
	 * @note Notes: Hard constraints; hidden when bOverrideRules is false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant",
		meta = (EditCondition = "bOverrideRules"))
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
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Defines the default spacing rules for this enemy item type.
	 * @note Technical: Applied when no variant overrides are selected.
	 * @note Notes: Hard filters; variants can override per-placement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ruleset")
	FEnemyItemPlacementRules BaseRules;

	/**
	 * How to select variants when multiple placements of this enemy item occur.
	 * Example: Use CycleByPlacementIndex to repeat a predictable pattern along a chain of anchors.
	 * @note Used in: EnemyObjectsPlaced variant selection.
	 * @note Why: Controls deterministic selection of which variant to apply for each placement.
	 * @note Technical: Selection mode affects ordering by placement index and attempt index.
	 * @note Notes: Drives the EditCondition for Variants; None disables the variant list.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ruleset")
	EEnemyRuleVariantSelectionMode VariantMode = EEnemyRuleVariantSelectionMode::None;

	/**
	 * Optional placement variants used based on VariantMode.
	 * Example: Add a variant with longer MinHopsFromEnemyHQ to create a late-campaign outpost.
	 * @note Used in: EnemyObjectsPlaced when VariantMode is not None.
	 * @note Why: Provides optional per-placement rule variants for this enemy item type.
	 * @note Technical: Indexed based on VariantMode and placement index for deterministic selection.
	 * @note Notes: Hidden when VariantMode is None; disabled variants are skipped.
	 */
	// Example: 3 variants -> placements 0,3,6 use Variants[0], 1,4,7 use [1], 2,5,8 use [2].
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ruleset",
		meta = (EditCondition = "VariantMode!=EEnemyRuleVariantSelectionMode::None"))
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
	 * @note Used in: EnemyObjectsPlaced.
	 * @note Why: Provides per-enemy-type rules to customize spacing and variants.
	 * @note Technical: The map key selects which ruleset applies to each item type.
	 * @note Notes: Missing keys fall back to default rules (if any); extra keys are ignored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	TMap<EMapEnemyItem, FEnemyItemRuleset> RulesByEnemyItem;
};
