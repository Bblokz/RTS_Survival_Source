// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PlayerWorldOutliner.h"

#include "RTS_Survival/Player/PlayerOutlineComponent/RTSOutlineHelpers/FRTSOutlineHelpers.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldNeutralObject/WorldNeutralObject.h"

UPlayerWorldOutliner::UPlayerWorldOutliner()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPlayerWorldOutliner::OnPlayerTick(AActor* HitActor, const FVector& HitLocation)
{
	(void)HitLocation;

	if (not IsValid(HitActor))
	{
		ResetOutlineOnPreviousActor();
		return;
	}

	const ERTSOutLineTypes OutlineType = GetOutlineTypeForActor(HitActor);
	if (OutlineType == ERTSOutLineTypes::None)
	{
		ResetOutlineOnPreviousActor();
		return;
	}

	ResetOutlineIfActorChanged(HitActor);
	SetOutLineOnActor(HitActor, OutlineType);
	M_OutlinedActor = HitActor;
}

ERTSOutLineTypes UPlayerWorldOutliner::GetOutlineTypeForActor(const AActor* Actor) const
{
	if (not IsValid(Actor))
	{
		return ERTSOutLineTypes::None;
	}

	if (Actor->IsA(AWorldMissionObject::StaticClass()))
	{
		return ERTSOutLineTypes::Radixite;
	}

	if (Actor->IsA(AWorldEnemyObject::StaticClass()))
	{
		return ERTSOutLineTypes::Metal;
	}

	if (Actor->IsA(AWorldNeutralObject::StaticClass()))
	{
		return ERTSOutLineTypes::VehicleParts;
	}

	return ERTSOutLineTypes::None;
}

void UPlayerWorldOutliner::ResetOutlineOnPreviousActor()
{
	const AActor* OutlinedActor = M_OutlinedActor.Get();
	if (not IsValid(OutlinedActor))
	{
		M_OutlinedActor = nullptr;
		return;
	}

	ResetActorOutline(OutlinedActor);
	M_OutlinedActor = nullptr;
}

void UPlayerWorldOutliner::ResetOutlineIfActorChanged(const AActor* NewActor)
{
	if (not IsValid(NewActor))
	{
		ResetOutlineOnPreviousActor();
		return;
	}

	const AActor* OutlinedActor = M_OutlinedActor.Get();
	if (not IsValid(OutlinedActor))
	{
		return;
	}

	if (OutlinedActor != NewActor)
	{
		ResetActorOutline(OutlinedActor);
		M_OutlinedActor = nullptr;
	}
}

void UPlayerWorldOutliner::SetOutLineOnActor(const AActor* ActorToOutLine, const ERTSOutLineTypes OutLineType) const
{
	if (not IsValid(ActorToOutLine))
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	ActorToOutLine->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (not IsValid(PrimitiveComponent))
		{
			continue;
		}

		FRTSOutlineHelpers::SetRTSOutLineOnComponent(PrimitiveComponent, OutLineType);
	}
}

void UPlayerWorldOutliner::ResetActorOutline(const AActor* ActorToReset) const
{
	if (not IsValid(ActorToReset))
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	ActorToReset->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (not IsValid(PrimitiveComponent))
		{
			continue;
		}

		FRTSOutlineHelpers::SetRTSOutLineOnComponent(PrimitiveComponent, ERTSOutLineTypes::None);
	}
}
