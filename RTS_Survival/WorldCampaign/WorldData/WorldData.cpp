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

bool UWorldData::TryGetFortificationStrengthDefinition(
	const EWorldFortificationStrength FortificationStrength,
	const ERTSGameDifficulty GameDifficulty,
	FWorldDataStrengthReasonDefinition& OutDefinition) const
{
	const FWorldDataStrengthReasonDefinition* Definition =
		M_FortificationStrengthDefinitions.Find(FortificationStrength);
	if (Definition == nullptr)
	{
		OutDefinition = FWorldDataStrengthReasonDefinition();
		return false;
	}

	OutDefinition = *Definition;
	OutDefinition.StrengthPercentage = M_FortificationStrengthMlt.ApplyToPercentage(
		Definition->StrengthPercentage,
		GameDifficulty);
	return true;
}

bool UWorldData::TryGetStrategicSupportDefinition(
	const EWorldStrategicSupport StrategicSupport,
	const ERTSGameDifficulty GameDifficulty,
	FWorldDataStrengthReasonDefinition& OutDefinition) const
{
	const FWorldDataStrengthReasonDefinition* Definition = M_StrategicSupportDefinitions.Find(StrategicSupport);
	if (Definition == nullptr)
	{
		OutDefinition = FWorldDataStrengthReasonDefinition();
		return false;
	}

	OutDefinition = *Definition;
	OutDefinition.StrengthPercentage = M_StrategicSupportMlt.ApplyToPercentage(
		Definition->StrengthPercentage,
		GameDifficulty);
	return true;
}

bool UWorldData::TryGetFieldDivisionDefinition(
	const EWorldFieldDivisions FieldDivision,
	const ERTSGameDifficulty GameDifficulty,
	FWorldDataStrengthReasonDefinition& OutDefinition) const
{
	const FWorldDataStrengthReasonDefinition* Definition = M_FieldDivisionDefinitions.Find(FieldDivision);
	if (Definition == nullptr)
	{
		OutDefinition = FWorldDataStrengthReasonDefinition();
		return false;
	}

	OutDefinition = *Definition;
	OutDefinition.StrengthPercentage = M_FieldDivisionMlt.ApplyToPercentage(
		Definition->StrengthPercentage,
		GameDifficulty);
	return true;
}
