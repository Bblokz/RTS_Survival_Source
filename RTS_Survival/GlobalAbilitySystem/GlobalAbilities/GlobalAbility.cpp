// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GlobalAbility.h"

#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"

UGlobalAbility::UGlobalAbility()
{
}

void UGlobalAbility::InitGlobalAbility(const int32 OwningPlayer,
                                       TWeakObjectPtr<UGlobalAbilitiesManager> GlobalAbilitiesManager,
                                       ACPPController* PlayerController)
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

void UGlobalAbility::OnClickedAbilityLocation(const FVector& TargetLocation)
{
	if (not EnsureIsValidGlobalAbilityManager())
	{
		return;
	}
	// see if the ability can actually be executed given the requirements using the manager.
	
}

void UGlobalAbility::ActivateAbility()
{
	if (M_AbilityState == EGlobalAbilityState::Activated)
	{
		// Already Active!
		return;
	}
	if (IsBlocked())
	{
		return;
	}
	// todo if owned by player 1; 
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

bool UGlobalAbility::IsBlocked()
{
	if (IsBlockedByRequirements() || IsBlockedByCooldown() || IsBlockedByCosts())
	{
		return true;
	}
	return false;
}

bool UGlobalAbility::IsBlockedByRequirements()
{
	if (not IsOwnedByPlayer())
	{
		// Enemy controller does not use requirements.
		return false;
	}
	if (not EnsureIsValidGlobalAbilityManager())
	{
		return true;
	}
	return M_GlobalAbilitiesManager->QueryRequirementForAbility(this);
}

bool UGlobalAbility::IsBlockedByCosts()
{
	if (not IsOwnedByPlayer())
	{
		// Enemy controller does not use costs.
		return false;
	}
	if (not EnsureIsValidGlobalAbilityManager())
	{
		return true;
	}
	return M_GlobalAbilitiesManager->QueryCostsForAbility(this);
}

bool UGlobalAbility::IsBlockedByCooldown()
{
	if (not IsOwnedByPlayer())
	{
		// Enemy controller does not use cooldown.
		return false;
	}
	return M_AbilityCosts.CoolDownRemaining > 0;
}
