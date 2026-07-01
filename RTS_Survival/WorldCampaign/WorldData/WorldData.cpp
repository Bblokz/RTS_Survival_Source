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
