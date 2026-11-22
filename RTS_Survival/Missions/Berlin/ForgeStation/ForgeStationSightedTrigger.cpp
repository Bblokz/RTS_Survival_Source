// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ForgeStationSightedTrigger.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Missions/Berlin/ForgeStation/BerlinForgeStation.h"

bool UForgeStationSightedTrigger::CheckTrigger_Implementation() const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const double Now = World->GetTimeSeconds();
	if (M_LastCheckSeconds >= 0.0 && Now - M_LastCheckSeconds < CheckIntervalSeconds)
	{
		return false;
	}
	M_LastCheckSeconds = Now;
	const ABerlinForgeStation* Station = ABerlinForgeStation::FindForgeStation(World);
	if (IsValid(Station) && Station->GetAliveTargetCount() < Station->GetTargetCount())
	{
		return true;
	}
	const FVector Centre = IsValid(Station) ? Station->GetActorLocation() : FallbackCentre;
	return GetIsPlayerUnitInRadius(*World, Centre);
}

bool UForgeStationSightedTrigger::GetIsPlayerUnitInRadius(const UWorld& World, const FVector& Centre) const
{
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	FCollisionQueryParams QueryParams(TEXT("ForgeStationSighted"), false);
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
