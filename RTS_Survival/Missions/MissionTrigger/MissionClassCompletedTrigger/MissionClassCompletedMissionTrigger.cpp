// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionClassCompletedMissionTrigger.h"

#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

bool UMissionClassCompletedMissionTrigger::CheckTrigger_Implementation() const
{
	if (not MissionClassToWatch)
	{
		RTSFunctionLibrary::ReportError(
			"Mission class completed trigger has no MissionClassToWatch configured."
			"\n Trigger : " + GetName());
		return false;
	}

	AMissionManager* MissionManager = GetMissionManagerCheckedFromTrigger();
	if (not IsValid(MissionManager))
	{
		return false;
	}
	if (MinimumDifficulty != ERTSGameDifficulty::NewToRTS)
	{
		ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
		if (not EnemyPlacement::TryGetGameDifficulty(MissionManager, Difficulty) || Difficulty < MinimumDifficulty)
		{
			return false;
		}
	}

	if (GetHasClassEnded(*MissionManager, MissionClassToWatch))
	{
		return true;
	}
	for (const TSubclassOf<UMissionBase>& AlsoWatchedClass : AlsoWatchMissionClasses)
	{
		if (AlsoWatchedClass && GetHasClassEnded(*MissionManager, AlsoWatchedClass))
		{
			return true;
		}
	}
	return false;
}

bool UMissionClassCompletedMissionTrigger::GetHasClassEnded(const AMissionManager& MissionManager,
                                                            const TSubclassOf<UMissionBase>& MissionClass) const
{
	if (MissionManager.GetHasCompletedMissionClassExact(MissionClass))
	{
		return true;
	}
	return bAlsoTriggerWhenFailed && MissionManager.GetHasFailedMissionClassExact(MissionClass);
}

