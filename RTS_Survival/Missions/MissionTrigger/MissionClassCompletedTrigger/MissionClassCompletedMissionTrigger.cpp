// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionClassCompletedMissionTrigger.h"

#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
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

	return MissionManager->GetHasCompletedMissionClassExact(MissionClassToWatch);
}

