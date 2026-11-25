// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DirectionFindingSubsystem.h"

#include "DirectionFindingManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UDirectionFindingSubsystem::RegisterManager(ADirectionFindingManager* Manager)
{
	if (M_Manager.IsValid() && M_Manager.Get() != Manager)
	{
		RTSFunctionLibrary::ReportError(TEXT("A second direction-finding manager registered; only ") +
			Manager->GetName() + TEXT(" is used now. Keep one per map."));
	}
	M_Manager = Manager;
}

void UDirectionFindingSubsystem::UnregisterManager(const ADirectionFindingManager* Manager)
{
	if (M_Manager.Get() == Manager)
	{
		M_Manager.Reset();
	}
}
