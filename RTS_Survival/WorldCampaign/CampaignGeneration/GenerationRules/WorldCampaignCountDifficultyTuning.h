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
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides",
		meta = (ToolTip = R"(Used in: EnemyObjectsPlaced count setup.
Why: Allows per-difficulty tuning by adding extra enemy items by type.
Technical: Added after base count scaling; treated as a hard increase.
Notes: Deterministic given DifficultyLevel and Seed; affects required counts.)"))
	TMap<EMapEnemyItem, int32> ExtraEnemyItemsByType;

	/**
	 * Extra neutral items to add by type on top of base counts.
	 * Example: Add +1 RadixiteField on Hard to increase resource pressure.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides",
		meta = (ToolTip = R"(Used in: NeutralObjectsPlaced count setup.
Why: Allows per-difficulty tuning by adding extra neutral items by type.
Technical: Added after base count scaling; treated as a hard increase.
Notes: Deterministic given DifficultyLevel and Seed; affects required counts.)"))
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
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides",
		meta = (ToolTip = R"(Used in: Count overrides lookup when DifficultyLevel == NewToRTS.
Why: Keeps beginner runs simpler by controlling extra spawn counts.
Technical: Returned by GetDifficultyOverrides and added to scaled base counts.
Notes: Hard add-on; negative values should be avoided.)"))
	FDifficultyEnumItemOverrides NewToRTS;

	/**
	 * Overrides applied for Normal difficulty.
	 * Example: Add a small neutral item bonus to encourage exploration without heavy combat.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides",
		meta = (ToolTip = R"(Used in: Count overrides lookup when DifficultyLevel == Normal.
Why: Defines baseline extra items for the standard difficulty tier.
Technical: Returned by GetDifficultyOverrides and added to scaled base counts.
Notes: Hard add-on; values directly affect required placement counts.)"))
	FDifficultyEnumItemOverrides Normal;

	/**
	 * Overrides applied for Hard difficulty.
	 * Example: Add extra enemy outposts to tighten the mid-campaign challenge.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides",
		meta = (ToolTip = R"(Used in: Count overrides lookup when DifficultyLevel == Hard.
Why: Pushes extra content for a harder campaign curve.
Technical: Returned by GetDifficultyOverrides and added to scaled base counts.
Notes: Hard add-on; excessive values can cause placement failures.)"))
	FDifficultyEnumItemOverrides Hard;

	/**
	 * Overrides applied for Brutal difficulty.
	 * Example: Increase both enemy factories and checkpoints to create layered defenses.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides",
		meta = (ToolTip = R"(Used in: Count overrides lookup when DifficultyLevel == Brutal.
Why: Defines aggressive extra counts for late-game challenge.
Technical: Returned by GetDifficultyOverrides and added to scaled base counts.
Notes: Hard add-on; verify there are enough anchors to place all items.)"))
	FDifficultyEnumItemOverrides Brutal;

	/**
	 * Overrides applied for Ironman difficulty.
	 * Example: Add a few extra enemy HQ-adjacent structures to punish direct routes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides",
		meta = (ToolTip = R"(Used in: Count overrides lookup when DifficultyLevel == Ironman.
Why: Defines extra counts for the most punishing tier.
Technical: Returned by GetDifficultyOverrides and added to scaled base counts.
Notes: Hard add-on; may trigger backtracking if counts exceed capacity.)"))
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
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty",
		meta = (ToolTip = R"(Used in: Count/difficulty scaling before EnemyObjectsPlaced and NeutralObjectsPlaced.
Why: Ensures count scaling is deterministic and repeatable across retries.
Technical: Seed feeds the scaling calculations and random streams used for counts.
Notes: Changing it changes required item counts even with the same map seed.)"))
	int32 Seed = 0;

	/**
	 * Raw difficulty percentage used to scale base counts.
	 * Example: Raise this to 150 to increase overall spawn counts by 50%.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty",
		meta = (ToolTip = R"(Used in: Count scaling before EnemyObjectsPlaced and NeutralObjectsPlaced.
Why: Primary tuning lever for overall content density.
Technical: Clamped to DifficultyPercentMin/Max, then compressed by LogCompressionStrength.
Notes: Hard multiplier; very high values can exceed anchor capacity.)"))
	int32 DifficultyPercentage = 100;

	/**
	 * Minimum clamp for DifficultyPercentage before compression.
	 * Example: Raise this if you never want the campaign to scale below a baseline density.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty",
		meta = (ToolTip = R"(Used in: DifficultyPercentage clamping.
Why: Prevents difficulty scaling from dropping below a baseline.
Technical: Applied as a hard clamp before logarithmic compression.
Notes: Must be <= DifficultyPercentMax; affects all difficulties equally.)"))
	int32 DifficultyPercentMin = WorldCampaignDifficultyDefaults::DifficultyPercentMin;

	/**
	 * Maximum clamp for DifficultyPercentage before compression.
	 * Example: Lower this to prevent extreme content spikes on very high difficulty values.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty",
		meta = (ToolTip = R"(Used in: DifficultyPercentage clamping.
Why: Prevents extreme scaling spikes from overpopulating the map.
Technical: Applied as a hard clamp before logarithmic compression.
Notes: Must be >= DifficultyPercentMin; too low may flatten difficulty differences.)"))
	int32 DifficultyPercentMax = WorldCampaignDifficultyDefaults::DifficultyPercentMax;

	/**
	 * Strength of logarithmic compression applied to the difficulty percentage.
	 * Example: Increase this to soften the impact of very high difficulty values.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty",
		meta = (ToolTip = R"(Used in: Difficulty scaling.
Why: Softens the impact of high DifficultyPercentage values for smoother tuning.
Technical: Applied after clamping as a logarithmic compression factor.
Notes: Higher values reduce scaling variance; lower values preserve extremes.)"))
	float LogCompressionStrength = WorldCampaignDifficultyDefaults::LogCompressionStrength;

	/**
	 * Base enemy item counts before difficulty scaling.
	 * Example: Increase the base count of FortifiedCheckpoint to harden early routes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counts",
		meta = (ToolTip = R"(Used in: EnemyObjectsPlaced count setup.
Why: Defines the baseline number of each enemy item before scaling.
Technical: Scaled by DifficultyPercentage and then augmented by overrides.
Notes: Hard counts; too many may exceed available anchors and trigger backtracking.)"))
	TMap<EMapEnemyItem, int32> BaseEnemyItemsByType;

	/**
	 * Base neutral item counts before difficulty scaling.
	 * Example: Add more RuinedCity entries to emphasize scavenging in the mid campaign.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counts",
		meta = (ToolTip = R"(Used in: NeutralObjectsPlaced count setup.
Why: Defines the baseline number of each neutral item before scaling.
Technical: Scaled by DifficultyPercentage and then augmented by overrides.
Notes: Hard counts; excessive values can cause placement failure.)"))
	TMap<EMapNeutralObjectType, int32> BaseNeutralItemsByType;

	/**
	 * Selected difficulty tier to pull overrides from.
	 * Example: Set to Hard to apply the Hard overrides in DifficultyEnumOverrides.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty",
		meta = (ToolTip = R"(Used in: GetDifficultyOverrides for count setup.
Why: Selects which override set is applied on top of scaled base counts.
Technical: Switches to the matching FDifficultyEnumItemOverrides struct.
Notes: Deterministic; changing this changes required counts immediately.)"))
	ERTSGameDifficulty DifficultyLevel = ERTSGameDifficulty::Normal;

	/**
	 * Overrides table that augments base counts per difficulty tier.
	 * Example: Add extra neutral items for NewToRTS while increasing enemy items for Brutal.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty",
		meta = (ToolTip = R"(Used in: Difficulty-level count overrides.
Why: Central place to author per-difficulty additions for enemy/neutral counts.
Technical: Queried by GetDifficultyOverrides based on DifficultyLevel.
Notes: Hard add-on to counts; does not respect placement capacity automatically.)"))
	FDifficultyEnumOverridesTable DifficultyEnumOverrides;
};
