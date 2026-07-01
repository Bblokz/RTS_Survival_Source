// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"

void AWorldEnemyObject::InitializeForAnchorWithEnemyItem(AAnchorPoint* AnchorPoint, EMapEnemyItem EnemyItemType)
{
	AWorldMapObject::InitializeForAnchor(AnchorPoint);
	M_EnemyItemType = EnemyItemType;
}

void AWorldEnemyObject::SetPrimaryReward(const FPrimaryReward& PrimaryReward)
{
	M_PrimaryReward = PrimaryReward;
}

void AWorldEnemyObject::SetSecondaryObjectiveData(const EBonusObjective BonusObjective,
                                                  const FText& SecondaryObjectiveText,
                                                  const FSecondaryReward& SecondaryReward)
{
	M_BonusObjective = BonusObjective;
	M_MapItemUIData.SecondaryObjectiveText = SecondaryObjectiveText;
	M_SecondaryReward = SecondaryReward;
}
