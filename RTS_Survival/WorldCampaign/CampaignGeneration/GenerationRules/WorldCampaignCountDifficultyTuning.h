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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	TMap<EMapEnemyItem, int32> ExtraEnemyItemsByType;

	/**
	 * Extra neutral items to add by type on top of base counts.
	 * Example: Add +1 RadixiteField on Hard to increase resource pressure.
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
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides NewToRTS;

	/**
	 * Overrides applied for Normal difficulty.
	 * Example: Add a small neutral item bonus to encourage exploration without heavy combat.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides Normal;

	/**
	 * Overrides applied for Hard difficulty.
	 * Example: Add extra enemy outposts to tighten the mid-campaign challenge.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides Hard;

	/**
	 * Overrides applied for Brutal difficulty.
	 * Example: Increase both enemy factories and checkpoints to create layered defenses.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides Brutal;

	/**
	 * Overrides applied for Ironman difficulty.
	 * Example: Add a few extra enemy HQ-adjacent structures to punish direct routes.
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
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	int32 Seed = 0;

	/**
	 * Raw difficulty percentage used to scale base counts.
	 * Example: Raise this to 150 to increase overall spawn counts by 50%.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	int32 DifficultyPercentage = 100;

	/**
	 * Minimum clamp for DifficultyPercentage before compression.
	 * Example: Raise this if you never want the campaign to scale below a baseline density.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	int32 DifficultyPercentMin = WorldCampaignDifficultyDefaults::DifficultyPercentMin;

	/**
	 * Maximum clamp for DifficultyPercentage before compression.
	 * Example: Lower this to prevent extreme content spikes on very high difficulty values.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	int32 DifficultyPercentMax = WorldCampaignDifficultyDefaults::DifficultyPercentMax;

	/**
	 * Strength of logarithmic compression applied to the difficulty percentage.
	 * Example: Increase this to soften the impact of very high difficulty values.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	float LogCompressionStrength = WorldCampaignDifficultyDefaults::LogCompressionStrength;

	/**
	 * Base enemy item counts before difficulty scaling.
	 * Example: Increase the base count of FortifiedCheckpoint to harden early routes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counts")
	TMap<EMapEnemyItem, int32> BaseEnemyItemsByType;

	/**
	 * Base neutral item counts before difficulty scaling.
	 * Example: Add more RuinedCity entries to emphasize scavenging in the mid campaign.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counts")
	TMap<EMapNeutralObjectType, int32> BaseNeutralItemsByType;

	/**
	 * Selected difficulty tier to pull overrides from.
	 * Example: Set to Hard to apply the Hard overrides in DifficultyEnumOverrides.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	ERTSGameDifficulty DifficultyLevel = ERTSGameDifficulty::Normal;

	/**
	 * Overrides table that augments base counts per difficulty tier.
	 * Example: Add extra neutral items for NewToRTS while increasing enemy items for Brutal.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FDifficultyEnumOverridesTable DifficultyEnumOverrides;
};
