// Copyright (C) Bas Blokzijl - All rights reserved.


#include "MissionTrigger.h"

#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UMissionTrigger::InitTrigger(UMissionBase* AssociatedMission)
{
	M_Mission = AssociatedMission;
}

void UMissionTrigger::TickTrigger()
{
	if (not CheckTrigger())
	{
		return;
	}

	if (not GetIsValidMission())
	{
		return;
	}

	M_Mission->OnTriggerActivated();
}

AMissionManager* UMissionTrigger::GetMissionManagerCheckedFromTrigger() const
{
	if (not GetIsValidMission())
	{
		return nullptr;
	}

	return M_Mission->GetMissionManagerChecked();
}

bool UMissionTrigger::GetIsValidMission() const
{
	if (not M_Mission.IsValid())
	{
		RTSFunctionLibrary::ReportError("Trigger: + " + GetName() + " has no valid mission.");
		return false;
	}

	return true;
}
