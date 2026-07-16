#include "RTS_Survival/Game/RTSGameInstance/WorldMissionContext/WorldMissionContext.h"

int32 FWorldMissionContext::GetTotalStrengthPercentage() const
{
	int32 TotalStrengthPercentage = 0;
	for (const FWorldStrengthContribution& StrengthContribution : StrengthContributions)
	{
		TotalStrengthPercentage += StrengthContribution.StrengthPercentage;
	}

	return TotalStrengthPercentage;
}

bool FWorldMissionContext::GetIsValid() const
{
	if (not bIsInitialized)
	{
		return false;
	}

	if (OperationType == EMapItemType::EnemyItem)
	{
		return EnemyObjectType != EMapEnemyItem::None && MissionType == EMapMission::None;
	}

	if (OperationType == EMapItemType::Mission)
	{
		return MissionType != EMapMission::None && EnemyObjectType == EMapEnemyItem::None;
	}

	return false;
}

int32 UWorldMissionContextBlueprintLibrary::GetTotalStrengthPercentage(
	const FWorldMissionContext& WorldMissionContext)
{
	return WorldMissionContext.GetTotalStrengthPercentage();
}

bool UWorldMissionContextBlueprintLibrary::GetIsWorldMissionContextValid(
	const FWorldMissionContext& WorldMissionContext)
{
	return WorldMissionContext.GetIsValid();
}
