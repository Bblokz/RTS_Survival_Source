// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldData/WorldData.h"

const FWorldDataEnemyPrimaryRewardPool* UWorldData::FindEnemyPrimaryRewardPool(
	const EMapEnemyItem EnemyItemType) const
{
	for (const FWorldDataEnemyPrimaryRewardPool& RewardPool : M_EnemyPrimaryRewardPools)
	{
		if (RewardPool.EnemyItemType == EnemyItemType)
		{
			return &RewardPool;
		}
	}

	return nullptr;
}

bool UWorldData::TryGetMissionBaseDifficultyPercentage(const EMapMission MissionType,
                                                       const ERTSGameDifficulty GameDifficulty,
                                                       int32& OutDifficultyPercentage) const
{
	const int32* BaseDifficultyPercentage = M_BaseDifficultyPercentageByMission.Find(MissionType);
	if (BaseDifficultyPercentage == nullptr)
	{
		OutDifficultyPercentage = 0;
		return false;
	}

	OutDifficultyPercentage = M_MissionBaseDifficultyMlt.ApplyToPercentage(
		*BaseDifficultyPercentage,
		GameDifficulty);
	return true;
}

bool UWorldData::TryGetEnemyBaseDifficultyPercentage(const EMapEnemyItem EnemyItemType,
                                                     const ERTSGameDifficulty GameDifficulty,
                                                     int32& OutDifficultyPercentage) const
{
	const int32* BaseDifficultyPercentage = M_BaseDifficultyPercentageByEnemyItem.Find(EnemyItemType);
	if (BaseDifficultyPercentage == nullptr)
	{
		OutDifficultyPercentage = 0;
		return false;
	}

	OutDifficultyPercentage = M_EnemyBaseDifficultyMlt.ApplyToPercentage(
		*BaseDifficultyPercentage,
		GameDifficulty);
	return true;
}
