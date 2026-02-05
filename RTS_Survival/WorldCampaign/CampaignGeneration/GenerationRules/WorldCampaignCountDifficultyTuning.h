#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"

#include "WorldCampaignCountDifficultyTuning.generated.h"

namespace WorldCampaignDifficultyDefaults
{
	constexpr int32 DifficultyPercentMin = 50;
	constexpr int32 DifficultyPercentMax = 2000;
	constexpr float LogCompressionStrength = 1.0f;
}

/**
 * @brief Per-difficulty overrides for enemy and neutral item counts, applied before
 *        EnemyObjectsPlaced and NeutralObjectsPlaced to scale content density.
 */
USTRUCT(BlueprintType)
struct FDifficultyEnumItemOverrides
{
	GENERATED_BODY()

	/**
	 * Extra enemy items to add by type on top of base counts.
	 * Example: Add +2 factories on Brutal to increase late-game production hubs.
	 * @note Used in: EnemyObjectsPlaced count setup.
	 * @note Why: Allows per-difficulty tuning by adding extra enemy items by type.
	 * @note Technical: Added after base count scaling; treated as a hard increase.
	 * @note Notes: Deterministic given DifficultyLevel and Seed; affects required counts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	TMap<EMapEnemyItem, int32> ExtraEnemyItemsByType;

	/**
	 * Extra neutral items to add by type on top of base counts.
	 * Example: Add +1 RadixiteField on Hard to increase resource pressure.
	 * @note Used in: NeutralObjectsPlaced count setup.
	 * @note Why: Allows per-difficulty tuning by adding extra neutral items by type.
	 * @note Technical: Added after base count scaling; treated as a hard increase.
	 * @note Notes: Deterministic given DifficultyLevel and Seed; affects required counts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	TMap<EMapNeutralObjectType, int32> ExtraNeutralItemsByType;
};

/**
 * @brief Lookup table mapping each difficulty tier to its count overrides, used during
 *        the setup that precedes EnemyObjectsPlaced and NeutralObjectsPlaced.
 */
USTRUCT(BlueprintType)
struct FDifficultyEnumOverridesTable
{
	GENERATED_BODY()

	/**
	 * Overrides applied for NewToRTS difficulty.
	 * Example: Keep this empty to avoid extra spawns for new players.
	 * @note Used in: Count overrides lookup when DifficultyLevel == NewToRTS.
	 * @note Why: Keeps beginner runs simpler by controlling extra spawn counts.
	 * @note Technical: Returned by GetDifficultyOverrides and added to scaled base counts.
	 * @note Notes: Hard add-on; negative values should be avoided.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides NewToRTS;

	/**
	 * Overrides applied for Normal difficulty.
	 * Example: Add a small neutral item bonus to encourage exploration without heavy combat.
	 * @note Used in: Count overrides lookup when DifficultyLevel == Normal.
	 * @note Why: Defines baseline extra items for the standard difficulty tier.
	 * @note Technical: Returned by GetDifficultyOverrides and added to scaled base counts.
	 * @note Notes: Hard add-on; values directly affect required placement counts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides Normal;

	/**
	 * Overrides applied for Hard difficulty.
	 * Example: Add extra enemy outposts to tighten the mid-campaign challenge.
	 * @note Used in: Count overrides lookup when DifficultyLevel == Hard.
	 * @note Why: Pushes extra content for a harder campaign curve.
	 * @note Technical: Returned by GetDifficultyOverrides and added to scaled base counts.
	 * @note Notes: Hard add-on; excessive values can cause placement failures.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides Hard;

	/**
	 * Overrides applied for Brutal difficulty.
	 * Example: Increase both enemy factories and checkpoints to create layered defenses.
	 * @note Used in: Count overrides lookup when DifficultyLevel == Brutal.
	 * @note Why: Defines aggressive extra counts for late-game challenge.
	 * @note Technical: Returned by GetDifficultyOverrides and added to scaled base counts.
	 * @note Notes: Hard add-on; verify there are enough anchors to place all items.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides Brutal;

	/**
	 * Overrides applied for Ironman difficulty.
	 * Example: Add a few extra enemy HQ-adjacent structures to punish direct routes.
	 * @note Used in: Count overrides lookup when DifficultyLevel == Ironman.
	 * @note Why: Defines extra counts for the most punishing tier.
	 * @note Technical: Returned by GetDifficultyOverrides and added to scaled base counts.
	 * @note Notes: Hard add-on; may trigger backtracking if counts exceed capacity.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides Ironman;
};

/**
 * @brief Tunable campaign count settings that influence how many enemy and neutral
 *        items are generated before EnemyObjectsPlaced and NeutralObjectsPlaced.
 */
USTRUCT(BlueprintType)
struct FWorldCampaignCountDifficultyTuning
{
	GENERATED_BODY()

	/**
	 * Seed used for deterministic difficulty scaling.
	 * Example: Set this to match a known seed when reproducing a specific campaign layout.
	 * @note Used in: Count/difficulty scaling before EnemyObjectsPlaced and NeutralObjectsPlaced.
	 * @note Why: Ensures count scaling is deterministic and repeatable across retries.
	 * @note Technical: Seed feeds the scaling calculations and random streams used for counts.
	 * @note Notes: Changing it changes required item counts even with the same map seed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	int32 Seed = 0;

	/**
	 * Raw difficulty percentage used to scale difficulty percentage of missions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	int32 DifficultyPercentage = 100;

	// How much extra base difficulty percentage is added when this option was turned on for the campaign generation.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	int32 AddedDifficultyPercentage = 25;

	/**
	 * Base enemy item counts before difficulty scaling.
	 * Example: Increase the base count of FortifiedCheckpoint to harden early routes.
	 * @note Used in: EnemyObjectsPlaced count setup.
	 * @note Why: Defines the baseline number of each enemy item before scaling.
	 * @note Technical: Scaled by DifficultyPercentage and then augmented by overrides.
	 * @note Notes: Hard counts; too many may exceed available anchors and trigger backtracking.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counts")
	TMap<EMapEnemyItem, int32> BaseEnemyItemsByType;

	/**
	 * Base neutral item counts before difficulty scaling.
	 * Example: Add more RuinedCity entries to emphasize scavenging in the mid campaign.
	 * @note Used in: NeutralObjectsPlaced count setup.
	 * @note Why: Defines the baseline number of each neutral item before scaling.
	 * @note Technical: Scaled by DifficultyPercentage and then augmented by overrides.
	 * @note Notes: Hard counts; excessive values can cause placement failure.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counts")
	TMap<EMapNeutralObjectType, int32> BaseNeutralItemsByType;

	/**
	 * Selected difficulty tier to pull overrides from.
	 * Example: Set to Hard to apply the Hard overrides in DifficultyEnumOverrides.
	 * @note Used in: GetDifficultyOverrides for count setup.
	 * @note Why: Selects which override set is applied on top of scaled base counts.
	 * @note Technical: Switches to the matching FDifficultyEnumItemOverrides struct.
	 * @note Notes: Deterministic; changing this changes required counts immediately.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	ERTSGameDifficulty DifficultyLevel = ERTSGameDifficulty::Normal;

	/**
	 * Overrides table that augments base counts per difficulty tier.
	 * Example: Add extra neutral items for NewToRTS while increasing enemy items for Brutal.
	 * @note Used in: Difficulty-level count overrides.
	 * @note Why: Central place to author per-difficulty additions for enemy/neutral counts.
	 * @note Technical: Queried by GetDifficultyOverrides based on DifficultyLevel.
	 * @note Notes: Hard add-on to counts; does not respect placement capacity automatically.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FDifficultyEnumOverridesTable DifficultyEnumOverrides;
};
