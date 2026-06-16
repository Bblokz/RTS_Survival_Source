// Copyright (C) Bas Blokzijl - All rights reserved.


#include "CommanderSettings.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

UCommanderSettings::UCommanderSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("CommanderSettings");
}

FRTSCommanderSettings UCommanderSettings::GetCommanderSettingsForType(const ERTSCommander CommanderType) const
{
	if (not CommanderSettingsByType.Contains(CommanderType))
	{
		RTSFunctionLibrary::ReportError("Could not find commander settings and abilities  for commander:"
								  "\n CommanderType " + UEnum::GetValueAsString(CommanderType));
		return FRTSCommanderSettings();
	}
	return CommanderSettingsByType[CommanderType];
}
