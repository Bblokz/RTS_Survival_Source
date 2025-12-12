// Copyright (C) Bas Blokzijl - All rights reserved.


#include "MissionTrigger.h"

#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UMissionTrigger::InitTrigger(UMissionBase* AssociatedMission)
{
	M_Mission = AssociatedMission;
}

void UMissionTrigger::TickTrigger()
{
	if(CheckTrigger() && EnsureMissionIsValid())
	{
		M_Mission->OnTriggerActivated();	
	}
}

bool UMissionTrigger::EnsureMissionIsValid() const
{
	if(M_Mission.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Trigger: + " + GetName() + " has no valid mission.");
	return false;
}
