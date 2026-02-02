// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"

#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"

AWorldMapObject::AWorldMapObject()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWorldMapObject::InitializeForAnchor(AAnchorPoint* AnchorPoint)
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	M_OwningAnchor = AnchorPoint;
	M_AnchorKey = AnchorPoint->GetAnchorKey();
}

FString AWorldMapObject::GetObjectDebugName() const
{
	return GetName();
}

AAnchorPoint* AWorldMapObject::GetOwningAnchor() const
{
	return M_OwningAnchor.Get();
}

FGuid AWorldMapObject::GetAnchorKey() const
{
	return M_AnchorKey;
}
