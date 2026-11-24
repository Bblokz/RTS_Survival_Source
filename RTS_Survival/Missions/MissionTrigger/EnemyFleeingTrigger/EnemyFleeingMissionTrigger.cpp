// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyFleeingMissionTrigger.h"

#include "RTS_Survival/Enemy/Fallback/EnemyFallbackSubsystem.h"

bool UEnemyFleeingMissionTrigger::CheckTrigger_Implementation() const
{
	const UEnemyFallbackSubsystem* FallbackSubsystem = UEnemyFallbackSubsystem::GetEnemyFallbackSubsystem(this);
	if (not IsValid(FallbackSubsystem))
	{
		return false;
	}
	return FallbackSubsystem->GetTotalFledCount() > 0;
}
