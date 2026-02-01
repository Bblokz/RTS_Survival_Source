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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced.\n"
		                  "Why: Prevents different enemy item types from stacking too tightly.\n"
		                  "Technical: Hard minimum hop distance filter between different item types.\n"
		                  "Notes: Larger values reduce clustering but can cause placement failures."))
	int32 MinEnemySeparationHopsOtherType = 0;

	/**
	 * Minimum hop distance between enemy items of the same type.
	 * Example: Raise this to spread out repeated barracks placements and reduce stacked
	 * defenses in a single region.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced.\n"
		                  "Why: Spreads identical enemy items so repeated defenses are not adjacent.\n"
		                  "Technical: Hard minimum hop distance filter between same-type items.\n"
		                  "Notes: High values reduce repetition but can starve placement candidates."))
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced.\n"
		                  "Why: Enforces a buffer around the enemy HQ for certain item types.\n"
		                  "Technical: Hard minimum hop distance from EnemyHQAnchor.\n"
		                  "Notes: Must be <= MaxHopsFromEnemyHQ; too high can remove valid anchors."))
	int32 MinHopsFromEnemyHQ = 0;

	/**
	 * Maximum hop distance from the enemy HQ for this item.
	 * Example: Lower this to keep high-value targets within a compact defensive ring.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced.\n"
		                  "Why: Keeps certain enemy items within a bounded radius of the enemy HQ.\n"
		                  "Technical: Hard maximum hop distance from EnemyHQAnchor.\n"
		                  "Notes: Must be >= MinHopsFromEnemyHQ; too low can block placement."))
	int32 MaxHopsFromEnemyHQ = 0;

	/**
	 * Preference for biasing placement within the Min/Max hop bounds.
	 * Example: PreferNearMaxBound to push an enemy outpost toward the edge of the enemy network.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced candidate ordering.\n"
		                  "Why: Biases placements toward the near/min or far/max side of the hop window.\n"
		                  "Technical: Soft preference used in ordering; does not bypass hard hop bounds.\n"
		                  "Notes: Deterministic ordering also uses anchor keys and attempt index."))
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced.\n"
		                  "Why: Defines item-to-item spacing constraints for this enemy item type.\n"
		                  "Technical: Hard hop-distance filters applied against existing enemy placements.\n"
		                  "Notes: Violations cause candidate anchors to be rejected."))
	FEnemyItemToItemSpacingRules ItemSpacing;

	/**
	 * Spacing rules relative to the enemy HQ.
	 * Example: Widen these to ensure certain enemy items appear as outer-ring objectives.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced.\n"
		                  "Why: Enforces min/max hop distance from the enemy HQ for this item type.\n"
		                  "Technical: Hard hop-distance filter based on EnemyHQHopDistancesByAnchorKey.\n"
		                  "Notes: If EnemyHQHopDistances is missing, this filter can fail placements."))
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced variant selection.\n"
		                  "Why: Allows designers to disable a variant without deleting it.\n"
		                  "Technical: Hard gate; disabled variants are skipped during selection.\n"
		                  "Notes: Deterministic selection ignores disabled entries entirely."))
	bool bEnabled = true;

	/**
	 * When true, use OverrideRules instead of the base rules for this placement.
	 * Example: Enable this to create a rare layout where a factory is allowed closer to the HQ.
	 */
	// If true, use these rules instead of base for this placement.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced variant selection.\n"
		                  "Why: Lets this variant override the base rules for specific placements.\n"
		                  "Technical: When true, OverrideRules are applied as the hard filters for this placement.\n"
		                  "Notes: Drives the EditCondition for OverrideRules; disabling hides the override field."))
	bool bOverrideRules = false;

	/**
	 * The rules to use when this variant overrides the base rules.
	 * Example: Set a shorter MinEnemySeparationHopsSameType to permit a clustered defense pair.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant",
		meta = (EditCondition = "bOverrideRules",
		        ToolTip = "Used in: EnemyObjectsPlaced when bOverrideRules is true.\n"
		                  "Why: Defines the variant-specific spacing rules for this placement.\n"
		                  "Technical: Replaces BaseRules for the selected placement index.\n"
		                  "Notes: Hard constraints; hidden when bOverrideRules is false."))
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ruleset",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced.\n"
		                  "Why: Defines the default spacing rules for this enemy item type.\n"
		                  "Technical: Applied when no variant overrides are selected.\n"
		                  "Notes: Hard filters; variants can override per-placement."))
	FEnemyItemPlacementRules BaseRules;

	/**
	 * How to select variants when multiple placements of this enemy item occur.
	 * Example: Use CycleByPlacementIndex to repeat a predictable pattern along a chain of anchors.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ruleset",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced variant selection.\n"
		                  "Why: Controls deterministic selection of which variant to apply for each placement.\n"
		                  "Technical: Selection mode affects ordering by placement index and attempt index.\n"
		                  "Notes: Drives the EditCondition for Variants; None disables the variant list."))
	EEnemyRuleVariantSelectionMode VariantMode = EEnemyRuleVariantSelectionMode::None;

	/**
	 * Optional placement variants used based on VariantMode.
	 * Example: Add a variant with longer MinHopsFromEnemyHQ to create a late-campaign outpost.
	 */
	// Example: 3 variants -> placements 0,3,6 use Variants[0], 1,4,7 use [1], 2,5,8 use [2].
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ruleset",
		meta = (EditCondition = "VariantMode!=EEnemyRuleVariantSelectionMode::None",
		        ToolTip = "Used in: EnemyObjectsPlaced when VariantMode is not None.\n"
		                  "Why: Provides optional per-placement rule variants for this enemy item type.\n"
		                  "Technical: Indexed based on VariantMode and placement index for deterministic selection.\n"
		                  "Notes: Hidden when VariantMode is None; disabled variants are skipped."))
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy",
		meta = (ToolTip = "Used in: EnemyObjectsPlaced.\n"
		                  "Why: Provides per-enemy-type rules to customize spacing and variants.\n"
		                  "Technical: The map key selects which ruleset applies to each item type.\n"
		                  "Notes: Missing keys fall back to default rules (if any); extra keys are ignored."))
	TMap<EMapEnemyItem, FEnemyItemRuleset> RulesByEnemyItem;
};
