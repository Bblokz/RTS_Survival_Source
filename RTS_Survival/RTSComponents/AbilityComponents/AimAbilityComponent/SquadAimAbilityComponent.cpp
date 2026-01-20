// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SquadAimAbilityComponent.h"

#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

USquadAimAbilityComponent::USquadAimAbilityComponent()
	: UAimAbilityComponent()
{
}

void USquadAimAbilityComponent::PostInitProperties()
{
	Super::PostInitProperties();
	M_SquadController = Cast<ASquadController>(GetOwner());
}

bool USquadAimAbilityComponent::CollectWeaponStates(TArray<UWeaponState*>& OutWeaponStates, float& OutMaxRange) const
{
	OutWeaponStates.Reset();
	OutMaxRange = 0.0f;
	if (not GetIsValidSquadController())
	{
		return false;
	}

	const TArray<ASquadUnit*> SquadUnits = M_SquadController->GetSquadUnitsChecked();
	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}
		AInfantryWeaponMaster* InfantryWeapon = SquadUnit->GetInfantryWeapon();
		if (not IsValid(InfantryWeapon))
		{
			continue;
		}
		const TArray<UWeaponState*> WeaponStates = InfantryWeapon->GetWeapons();
		for (UWeaponState* WeaponState : WeaponStates)
		{
			if (not IsValid(WeaponState))
			{
				continue;
			}
			OutWeaponStates.Add(WeaponState);
			OutMaxRange = FMath::Max(OutMaxRange, WeaponState->GetRange());
		}
	}

	if (OutWeaponStates.Num() > 0)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Aim ability could not find valid squad weapon states.");
	return false;
}

void USquadAimAbilityComponent::SetWeaponsDisabled()
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	const TArray<ASquadUnit*> SquadUnits = M_SquadController->GetSquadUnitsChecked();
	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}
		AInfantryWeaponMaster* InfantryWeapon = SquadUnit->GetInfantryWeapon();
		if (not IsValid(InfantryWeapon))
		{
			continue;
		}
		InfantryWeapon->DisableAllWeapons();
	}
}

void USquadAimAbilityComponent::SetWeaponsAutoEngage(const bool bUseLastTarget)
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	const TArray<ASquadUnit*> SquadUnits = M_SquadController->GetSquadUnitsChecked();
	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}
		SquadUnit->SetWeaponToAutoEngageTargets(bUseLastTarget);
	}
}

void USquadAimAbilityComponent::FireWeaponsAtLocation(const FVector& TargetLocation)
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	const TArray<ASquadUnit*> SquadUnits = M_SquadController->GetSquadUnitsChecked();
	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}
		AInfantryWeaponMaster* InfantryWeapon = SquadUnit->GetInfantryWeapon();
		if (not IsValid(InfantryWeapon))
		{
			continue;
		}
		InfantryWeapon->SetEngageGroundLocation(TargetLocation);
	}
}

void USquadAimAbilityComponent::RequestMoveToLocation(const FVector& MoveToLocation)
{
	if (not GetIsValidSquadController())
	{
		return;
	}
	M_SquadController->RequestSquadMoveForAbility(MoveToLocation, EAbilityID::IdAimAbility);
}

void USquadAimAbilityComponent::StopMovementForAbility()
{
	if (not GetIsValidSquadController())
	{
		return;
	}
	M_SquadController->StopMovement();
}

bool USquadAimAbilityComponent::GetIsValidSquadController() const
{
	if (not M_SquadController.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_SquadController",
		                                                      "USquadAimAbilityComponent::GetIsValidSquadController",
		                                                      this);
		return false;
	}

	return true;
}
