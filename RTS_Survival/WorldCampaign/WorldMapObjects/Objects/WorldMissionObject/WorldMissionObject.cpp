// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"

#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldFortificationModificationsComponent.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthEstimationComponent.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldCampaignMissionNames.h"

AWorldMissionObject::AWorldMissionObject()
{
	M_FortificationModificationsComponent =
		CreateDefaultSubobject<UWorldFortificationModificationsComponent>(TEXT("FortificationModifications"));
}

void AWorldMissionObject::InitializeForAnchorWithMissionType(AAnchorPoint* AnchorPoint, EMapMission MissionType)
{
	AWorldMapObject::InitializeForAnchor(AnchorPoint);
	M_MissionType = MissionType;
}

FText AWorldMissionObject::GetMissionDisplayName() const
{
	return WorldCampaignMissionNames::GetMissionDisplayName(M_MissionType);
}

FEnemyOrMissionMapItemUIData AWorldMissionObject::GetMapItemUIData() const
{
	return BuildMapItemUIData();
}

int32 AWorldMissionObject::GetBaseDifficultyPercentage() const
{
	if (const UWorldStrengthEstimationComponent* StrengthEstimationComponent = GetWorldStrengthEstimationComponent())
	{
		return StrengthEstimationComponent->GetBaseFortificationStrengthPercentage();
	}

	return 0;
}

void AWorldMissionObject::ResetStrategicReport()
{
	if (UWorldStrengthEstimationComponent* StrengthEstimationComponent = GetWorldStrengthEstimationComponent())
	{
		StrengthEstimationComponent->ResetStrategicSupportReport();
	}
}

void AWorldMissionObject::AddStrategicSupportReason(
	const FWorldStrengthReason& StrengthReason)
{
	if (UWorldStrengthEstimationComponent* StrengthEstimationComponent = GetWorldStrengthEstimationComponent())
	{
		StrengthEstimationComponent->AddStrategicSupportReason(StrengthReason);
	}
}

FEnemyOrMissionMapItemUIData AWorldMissionObject::BuildMapItemUIData() const
{
	FEnemyOrMissionMapItemUIData UIData = M_MapItemUIData;
	// A story mission can stand in on another mission's Blueprint, so its own name wins over the authored title.
	const FText MissionName = GetMissionDisplayName();
	if (not MissionName.IsEmpty())
	{
		UIData.TitleText = WorldCampaignMissionNames::MakeMissionTitleText(M_MapItemUIData.TitleText, MissionName);
	}

	if (const UWorldStrengthEstimationComponent* StrengthEstimationComponent = GetWorldStrengthEstimationComponent())
	{
		UIData.SetStrengthEstimation(StrengthEstimationComponent->GetStrengthEstimationMessage());
	}

	return UIData;
}
