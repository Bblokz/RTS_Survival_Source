// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldMapObject.h"


#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthEstimationComponent.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldMapFowComponent.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"

AWorldMapObject::AWorldMapObject()
{
	PrimaryActorTick.bCanEverTick = false;
	M_WorldStrengthEstimationComponent = CreateDefaultSubobject<UWorldStrengthEstimationComponent>(
		TEXT("WorldStrengthEstimation"));
}

void AWorldMapObject::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	M_WorldMapFowComponent = FindComponentByClass<UWorldMapFowComponent>();
}

void AWorldMapObject::InitializeForAnchor(AAnchorPoint* AnchorPoint)
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	SetActorTransform(AnchorPoint->GetActorTransform());
	M_OwningAnchor = AnchorPoint;
	M_AnchorKey = AnchorPoint->GetAnchorKey();
}

void AWorldMapObject::SetIsClearedByPlayer()
{
	if (bM_IsClearedByPlayer)
	{
		return;
	}

	bM_IsClearedByPlayer = true;
	// Cleared objects must not catch clicks or hover outlines any more, whatever they look like.
	SetActorEnableCollision(false);
	if (bM_HideWhenClearedByPlayer)
	{
		SetActorHiddenInGame(true);
	}
	BP_OnClearedByPlayer();
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
