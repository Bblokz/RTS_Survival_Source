// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_GA_Item.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_GA_Item::SetupGa_Item(const TWeakObjectPtr<UGlobalAbility> GlobalAbility, UGlobalAbilitiesManager* GlobalAbilityManager)
{
	M_GlobalAbility = GlobalAbility;
	(void)EnsureIsValidAbility();
	M_GlobalAbilityManager = GlobalAbilityManager;
	(void)EnsureIsValidAbilityManager();
}

void UW_GA_Item::OnAbilityHovered(UGlobalAbility* HoveredAbility, const bool bIsHover)
{
	
}

void UW_GA_Item::OnClickedAbilityButton()
{
	if (not EnsureIsValidAbility() || not EnsureIsValidAbilityManager())
	{
		return;
	}
	M_GlobalAbilityManager->On
}

void UW_GA_Item::OnHoveredAbilityButton(const bool bIsHover)
{
	
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

bool UW_GA_Item::EnsureIsValidAbilityManager()
{
	if (not M_GlobalAbilityManager.IsValid())
	{
		
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_GlobalAbilityManager"),
			TEXT("UW_GA_Item::EnsureIsValidAbilityManager"),
			this
		);
		return false;
	}
	return true;
}
