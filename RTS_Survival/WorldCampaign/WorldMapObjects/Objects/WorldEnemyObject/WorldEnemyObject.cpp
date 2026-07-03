// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"

#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldFortificationModificationsComponent.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthEstimationComponent.h"

AWorldEnemyObject::AWorldEnemyObject()
{
	M_FortificationModificationsComponent =
		CreateDefaultSubobject<UWorldFortificationModificationsComponent>(TEXT("FortificationModifications"));
}

void AWorldEnemyObject::InitializeForAnchorWithEnemyItem(AAnchorPoint* AnchorPoint, EMapEnemyItem EnemyItemType)
{
	AWorldMapObject::InitializeForAnchor(AnchorPoint);
	M_EnemyItemType = EnemyItemType;
}

FEnemyOrMissionMapItemUIData AWorldEnemyObject::GetMapItemUIData() const
{
	return BuildMapItemUIData();
}

void AWorldEnemyObject::SetPrimaryReward(const FPrimaryReward& PrimaryReward)
{
	M_PrimaryReward = PrimaryReward;
}

int32 AWorldEnemyObject::GetBaseDifficultyPercentage() const
{
	if (const UWorldStrengthEstimationComponent* StrengthEstimationComponent = GetWorldStrengthEstimationComponent())
	{
		return StrengthEstimationComponent->GetBaseFortificationStrengthPercentage();
	}

	return 0;
}

void AWorldEnemyObject::ResetStrategicReport()
{
	if (UWorldStrengthEstimationComponent* StrengthEstimationComponent = GetWorldStrengthEstimationComponent())
	{
		StrengthEstimationComponent->ResetStrategicSupportReport();
	}
}

void AWorldEnemyObject::AddStrategicSupportReason(
	const FWorldStrengthReason& StrengthReason)
{
	if (UWorldStrengthEstimationComponent* StrengthEstimationComponent = GetWorldStrengthEstimationComponent())
	{
		StrengthEstimationComponent->AddStrategicSupportReason(StrengthReason);
	}
}

FEnemyOrMissionMapItemUIData AWorldEnemyObject::BuildMapItemUIData() const
{
	FEnemyOrMissionMapItemUIData UIData = M_MapItemUIData;
	if (const UWorldStrengthEstimationComponent* StrengthEstimationComponent = GetWorldStrengthEstimationComponent())
	{
		UIData.SetStrengthEstimation(StrengthEstimationComponent->GetStrengthEstimationMessage());
	}

	return UIData;
}

void AWorldEnemyObject::SetSecondaryObjectiveData(const EBonusObjective BonusObjective,
                                                  const FText& SecondaryObjectiveText,
                                                  const FSecondaryReward& SecondaryReward)
{
	M_BonusObjective = BonusObjective;
	M_MapItemUIData.SecondaryObjectiveText = SecondaryObjectiveText;
	M_SecondaryReward = SecondaryReward;
}
