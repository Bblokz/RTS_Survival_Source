// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldNeutralObject/WorldNeutralObject.h"

void AWorldNeutralObject::InitializeForAnchorWithNeutralObjectType(AAnchorPoint* AnchorPoint,
                                                                   EMapNeutralObjectType NeutralObjectType)
{
	AWorldMapObject::InitializeForAnchor(AnchorPoint);
	M_NeutralObjectType = NeutralObjectType;
}
