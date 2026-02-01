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

USTRUCT(BlueprintType)
struct FDifficultyEnumItemOverrides
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	TMap<EMapEnemyItem, int32> ExtraEnemyItemsByType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	TMap<EMapNeutralObjectType, int32> ExtraNeutralItemsByType;
};

USTRUCT(BlueprintType)
struct FDifficultyEnumOverridesTable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides NewToRTS;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides Hard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides Brutal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overrides")
	FDifficultyEnumItemOverrides Ironman;
};

USTRUCT(BlueprintType)
struct FWorldCampaignCountDifficultyTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	int32 DifficultyPercentage = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	int32 DifficultyPercentMin = WorldCampaignDifficultyDefaults::DifficultyPercentMin;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	int32 DifficultyPercentMax = WorldCampaignDifficultyDefaults::DifficultyPercentMax;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	float LogCompressionStrength = WorldCampaignDifficultyDefaults::LogCompressionStrength;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counts")
	TMap<EMapEnemyItem, int32> BaseEnemyItemsByType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counts")
	TMap<EMapNeutralObjectType, int32> BaseNeutralItemsByType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	ERTSGameDifficulty DifficultyLevel = ERTSGameDifficulty::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FDifficultyEnumOverridesTable DifficultyEnumOverrides;
};
