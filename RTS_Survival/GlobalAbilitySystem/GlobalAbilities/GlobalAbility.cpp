// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GlobalAbility.h"

#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UGlobalAbility::UGlobalAbility()
{
}

void UGlobalAbility::InitGlobalAbility(const int32 OwningPlayer,
                                       TWeakObjectPtr<UGlobalAbilitiesManager> GlobalAbilitiesManager,
                                       ACPPController* PlayerController)
{
	M_OwningPlayer = OwningPlayer;
	M_GlobalAbilitiesManager = GlobalAbilitiesManager;
	M_PlayerController = PlayerController;
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
	if (IsBlocked())
	{
		CancelAbilityActivation();
		return;
	}
	if (IsOwnedByPlayer() && not M_GlobalAbilitiesManager.Get()->TryPayForAbility(this))
	{
		CancelAbilityActivation();
		return;
	}
	ExecuteAbilityAtLocation(TargetLocation);
	M_AbilityState = EGlobalAbilityState::NotActivated;
	M_GlobalAbilitiesManager.Get()->OnAbilityFinishedExecuting(this);
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
	M_AbilityState = EGlobalAbilityState::Activated;
	if (IsOwnedByPlayer() && EnsureIsValidPlayerController())
	{
		M_PlayerController.Get()->OnGlobaAbilityActivated(M_AimSettings, M_AbilitySoundSettings, this);
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
	return not M_GlobalAbilitiesManager.Get()->QueryRequirementForAbility(this);
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
	return not M_GlobalAbilitiesManager.Get()->QueryCostsForAbility(this);
}

bool UGlobalAbility::EnsureIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerController"),
		TEXT("UGlobalAbility::EnsureIsValidPlayerController"),
		this
	);
	return false;
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
