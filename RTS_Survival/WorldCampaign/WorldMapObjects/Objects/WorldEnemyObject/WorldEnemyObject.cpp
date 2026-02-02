// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"

void AWorldEnemyObject::InitializeForAnchor(AAnchorPoint* AnchorPoint, EMapEnemyItem EnemyItemType)
{
	AWorldMapObject::InitializeForAnchor(AnchorPoint);
	M_EnemyItemType = EnemyItemType;
}
