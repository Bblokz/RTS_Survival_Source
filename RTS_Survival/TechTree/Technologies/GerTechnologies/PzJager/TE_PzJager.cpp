// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_PzJager.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UTE_PzJager::ApplyOnTank_Internal(ATankMaster* Tank)
{
	if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		if (IsValid(Tank))
		{
			RTSFunctionLibrary::PrintString("Applying pz jager effect to " + Tank->GetName());
		}
	}
}
