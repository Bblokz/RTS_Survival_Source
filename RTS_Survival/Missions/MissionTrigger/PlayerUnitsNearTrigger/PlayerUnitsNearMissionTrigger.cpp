// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PlayerUnitsNearMissionTrigger.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"

bool UPlayerUnitsNearMissionTrigger::CheckTrigger_Implementation() const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	if (not GetShouldRunCheck(*World))
	{
		return false;
	}
	return GetIsPlayerUnitInRadius(*World, GetCentre(*World));
}

bool UPlayerUnitsNearMissionTrigger::GetShouldRunCheck(const UWorld& World) const
{
	const double Now = World.GetTimeSeconds();
	if (M_LastCheckSeconds >= 0.0 && Now - M_LastCheckSeconds < CheckIntervalSeconds)
	{
		return false;
	}
	M_LastCheckSeconds = Now;
	return true;
}

FVector UPlayerUnitsNearMissionTrigger::GetCentre(const UWorld& World) const
{
	if (CentreActorTag.IsNone())
	{
		return CentreLocation;
	}
	if (not bM_HasSearchedCentreActor)
	{
		bM_HasSearchedCentreActor = true;
		TArray<AActor*> TaggedActors;
		UGameplayStatics::GetAllActorsWithTag(&World, CentreActorTag, TaggedActors);
		if (TaggedActors.Num() > 0)
		{
			M_CentreActor = TaggedActors[0];
		}
	}
	if (M_CentreActor.IsValid())
	{
		return M_CentreActor->GetActorLocation();
	}
	return CentreLocation;
}

bool UPlayerUnitsNearMissionTrigger::GetIsPlayerUnitInRadius(const UWorld& World, const FVector& Centre) const
{
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	const FCollisionQueryParams QueryParams(TEXT("PlayerUnitsNearMissionTrigger"), false);
	TArray<FOverlapResult> Overlaps;
	World.OverlapMultiByObjectType(Overlaps, Centre, FQuat::Identity, ObjectParams,
	                               FCollisionShape::MakeSphere(Radius), QueryParams);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (IsValid(Overlap.GetActor()))
		{
			return true;
		}
	}
	return false;
}
