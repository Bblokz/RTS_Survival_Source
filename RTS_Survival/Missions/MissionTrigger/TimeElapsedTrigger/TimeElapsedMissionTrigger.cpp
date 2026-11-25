// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TimeElapsedMissionTrigger.h"

#include "Engine/World.h"

bool UTimeElapsedMissionTrigger::CheckTrigger_Implementation() const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	return World->GetTimeSeconds() >= Seconds;
}
