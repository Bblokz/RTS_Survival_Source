// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_WorldUIHeader.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

bool UW_WorldUIHeader::EnsureButtonIsValid(const TObjectPtr<UButton>& ButtonToCheck)
{
	if(not IsValid(ButtonToCheck))
	{
		RTSFunctionLibrary::ReportError("Button Is not valid for WorldUIHeader, please check the widget blueprint.");
		return false;
	}
	return true;
}
