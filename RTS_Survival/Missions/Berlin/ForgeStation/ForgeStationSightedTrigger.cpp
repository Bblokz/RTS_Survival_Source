// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ForgeStationSightedTrigger.h"

#include "Engine/World.h"
#include "RTS_Survival/Missions/Berlin/ForgeStation/BerlinForgeStation.h"

namespace ForgeStationSightedTriggerConstants
{
	// Outermost guard post + guard reach + 10 m (BerlinForgeStation.md section 5).
	constexpr float RingRadius = 10000.f;
	// The station pivot on Ger_Berlin, used when no actor carries the tag.
	const FVector StationPivot(363.f, 1382.f, 0.f);
}

UForgeStationSightedTrigger::UForgeStationSightedTrigger()
{
	CentreActorTag = ABerlinForgeStation::StationTag;
	CentreLocation = ForgeStationSightedTriggerConstants::StationPivot;
	Radius = ForgeStationSightedTriggerConstants::RingRadius;
}

bool UForgeStationSightedTrigger::CheckTrigger_Implementation() const
{
	const UWorld* World = GetWorld();
	const ABerlinForgeStation* Station = IsValid(World) ? ABerlinForgeStation::FindForgeStation(World) : nullptr;
	if (IsValid(Station) && Station->GetAliveTargetCount() < Station->GetTargetCount())
	{
		return true;
	}
	return Super::CheckTrigger_Implementation();
}
