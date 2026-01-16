// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/RadiusAOEBehaviourComponent/RadiusRepairAura.h"

#include "RTS_Survival/RTSComponents/RepairComponent/RepairHelpers/RepairHelpers.h"

bool URadiusRepairAura::IsValidTarget(AActor* ValidActor) const
{
	return FRTSRepairHelpers::GetIsUnitValidForRepairs(ValidActor);
}
