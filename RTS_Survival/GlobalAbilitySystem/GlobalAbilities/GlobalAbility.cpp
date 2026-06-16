// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GlobalAbility.h"

UGlobalAbility::UGlobalAbility()
{
}

void UGlobalAbility::InitGlobalAbility(const int32 OwningPlayer, TWeakObjectPtr<UGlobalAbilitiesManager> GlobalAbilitiesManager)
{
	M_OwningPlayer = OwningPlayer;
	M_GlobalAbilitiesManager = GlobalAbilitiesManager;
	(void)EnsureIsValidGlobalAbilityManager();
		
}

void UGlobalAbility::OnClickedAbilityButton()
{
	ActivateAbility();
}

void UGlobalAbility::CancelAbilityActivation()
{
	M_AbilityState = EGlobalAbilityState::NotActivated;
}

void UGlobalAbility::ActivateAbility()
{
	if (M_AbilityState == EGlobalAbilityState::Activated)
	{
		// Already Active!
		return;
	}
	if (IsBlockedByRequirements())
	{
		return;
	}
	
	
}

void UGlobalAbility::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
}

bool UGlobalAbility::IsOwnedByPlayer() const
{
	return M_OwningPlayer == 1;
}

bool UGlobalAbility::EnsureIsValidGlobalAbilityManager() const
{
	if (not M_GlobalAbilitiesManager.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_GlobalAbilitiesManager"),
			TEXT("UGlobalAbility::EnsureIsValidGlobalAbilityManager"),
			this
		);
		return false;
	}
	return true;
		
}

bool UGlobalAbility::IsBlockedByRequirements()
{
	if (not IsOwnedByPlayer())
	{
		// Enemy controller does not use requirements.
		return false;
	}
	
	
}
