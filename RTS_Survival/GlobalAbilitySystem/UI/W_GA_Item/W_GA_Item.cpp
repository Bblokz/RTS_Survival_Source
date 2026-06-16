// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_GA_Item.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_GA_Item::SetupGa_Item(const TWeakObjectPtr<UGlobalAbility> GlobalAbility)
{
	M_GlobalAbility = GlobalAbility;
	(void)EnsureIsValidAbility();
}

bool UW_GA_Item::EnsureIsValidAbility()
{
	if (not M_GlobalAbility.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_GlobalAbility"),
			TEXT("UW_GA_Item::EnsureIsValidAbility"),
			this
		);
		return false;
	}
	return true;
}
