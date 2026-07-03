// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/BonusObjectives/BonusObjectives.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"
#include "RTS_Survival/WorldCampaign/WorldDifficulty/WorldDifficultyTypes.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_MissionReward/RewardStructs/FMissionRewardStructs.h"
#include "WorldData.generated.h"

/**
 * @brief Ordered primary reward pool used for one enemy map item type.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldDataEnemyPrimaryRewardPool
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Rewards")
	EMapEnemyItem EnemyItemType = EMapEnemyItem::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Rewards")
	TArray<FPrimaryReward> PrimaryRewards;
};

/**
 * @brief Bonus objective definition used to procedurally fill enemy secondary objectives.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldDataBonusObjectiveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Bonus Objectives")
	EBonusObjective BonusObjective = EBonusObjective::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Bonus Objectives")
	FText SecondaryObjectiveText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Bonus Objectives")
	FSecondaryReward SecondaryReward;
};

/**
 * @brief Percentage and display text for enum-driven world strength reasons.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldDataStrengthReasonDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength")
	int32 StrengthPercentage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength", meta = (MultiLine = true))
	FText ReasonText;
};

/**
 * @brief Designer-authored enemy data loaded into generated enemy map objects after placement.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UWorldData : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * @brief Finds the designer-authored primary reward pool for one enemy map item type.
	 * @param EnemyItemType Enemy map item type to look up.
	 * @return Reward pool pointer, or nullptr when no pool exists for the type.
	 */
	const FWorldDataEnemyPrimaryRewardPool* FindEnemyPrimaryRewardPool(EMapEnemyItem EnemyItemType) const;

	/**
	 * @brief Gets the difficulty-adjusted base fortification strength for one mission type.
	 * @param MissionType Mission type to look up.
	 * @param GameDifficulty Current campaign difficulty.
	 * @param OutDifficultyPercentage Output signed percentage after difficulty multiplier.
	 * @return true when a mission entry was found.
	 */
	bool TryGetMissionBaseDifficultyPercentage(EMapMission MissionType,
	                                           ERTSGameDifficulty GameDifficulty,
	                                           int32& OutDifficultyPercentage) const;

	/**
	 * @brief Gets the difficulty-adjusted base fortification strength for one enemy item type.
	 * @param EnemyItemType Enemy item type to look up.
	 * @param GameDifficulty Current campaign difficulty.
	 * @param OutDifficultyPercentage Output signed percentage after difficulty multiplier.
	 * @return true when an enemy entry was found.
	 */
	bool TryGetEnemyBaseDifficultyPercentage(EMapEnemyItem EnemyItemType,
	                                         ERTSGameDifficulty GameDifficulty,
	                                         int32& OutDifficultyPercentage) const;

	/**
	 * @brief Gets the difficulty-adjusted strength definition for a fortification modifier enum.
	 * @param FortificationStrength Fortification modifier to look up.
	 * @param GameDifficulty Current campaign difficulty.
	 * @param OutDefinition Output definition with percentage already multiplied for difficulty.
	 * @return true when a fortification definition was found.
	 */
	bool TryGetFortificationStrengthDefinition(EWorldFortificationStrength FortificationStrength,
	                                           ERTSGameDifficulty GameDifficulty,
	                                           FWorldDataStrengthReasonDefinition& OutDefinition) const;

	/**
	 * @brief Gets the difficulty-adjusted strength definition for a strategic support enum.
	 * @param StrategicSupport Strategic support type to look up.
	 * @param GameDifficulty Current campaign difficulty.
	 * @param OutDefinition Output definition with percentage already multiplied for difficulty.
	 * @return true when a strategic support definition was found.
	 */
	bool TryGetStrategicSupportDefinition(EWorldStrategicSupport StrategicSupport,
	                                      ERTSGameDifficulty GameDifficulty,
	                                      FWorldDataStrengthReasonDefinition& OutDefinition) const;

	/**
	 * @brief Returns all bonus objective definitions used when filling generated enemy items.
	 * @return Designer-authored bonus objective definitions.
	 */
	const TArray<FWorldDataBonusObjectiveDefinition>& GetBonusObjectiveDefinitions() const
	{
		return M_BonusObjectiveDefinitions;
	}

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Rewards",
		meta = (AllowPrivateAccess = "true"))
	TArray<FWorldDataEnemyPrimaryRewardPool> M_EnemyPrimaryRewardPools;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Bonus Objectives",
		meta = (AllowPrivateAccess = "true"))
	TArray<FWorldDataBonusObjectiveDefinition> M_BonusObjectiveDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Difficulty|Missions",
		meta = (AllowPrivateAccess = "true"))
	TMap<EMapMission, int32> M_BaseDifficultyPercentageByMission;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Difficulty|Missions",
		meta = (AllowPrivateAccess = "true"))
	FRTSPerDifficultyMlt M_MissionBaseDifficultyMlt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Difficulty|Enemy Objects",
		meta = (AllowPrivateAccess = "true"))
	TMap<EMapEnemyItem, int32> M_BaseDifficultyPercentageByEnemyItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Difficulty|Enemy Objects",
		meta = (AllowPrivateAccess = "true"))
	FRTSPerDifficultyMlt M_EnemyBaseDifficultyMlt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength|Fortification Modifiers",
		meta = (AllowPrivateAccess = "true"))
	TMap<EWorldFortificationStrength, FWorldDataStrengthReasonDefinition> M_FortificationStrengthDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength|Fortification Modifiers",
		meta = (AllowPrivateAccess = "true"))
	FRTSPerDifficultyMlt M_FortificationStrengthMlt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength|Strategic Support",
		meta = (AllowPrivateAccess = "true"))
	TMap<EWorldStrategicSupport, FWorldDataStrengthReasonDefinition> M_StrategicSupportDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength|Strategic Support",
		meta = (AllowPrivateAccess = "true"))
	FRTSPerDifficultyMlt M_StrategicSupportMlt;
};
