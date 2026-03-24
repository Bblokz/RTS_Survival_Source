#include "TowedActor.h"

#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/TowMechanic/VehicleTow/VehicleTow.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UTowedActorComponent::UTowedActorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UTowedActorComponent::IsTowFree() const
{
	return not GetIsValidTowingActor() || not GetIsValidTowingVehicleTowComp();
}

bool UTowedActorComponent::GetIsValidTowingActor() const
{
	if (M_TowingActor.IsValid())
	{
		return true;
	}

	return false;
}

bool UTowedActorComponent::GetIsValidTowingVehicleTowComp() const
{
	if (M_TowingVehicleTowComp.IsValid())
	{
		return true;
	}

	return false;
}

void UTowedActorComponent::SetTowRelationship(AActor* TowingActor, UVehicleTowComponent* TowingVehicleTowComp)
{
	M_TowingActor = TowingActor;
	M_TowingVehicleTowComp = TowingVehicleTowComp;
}

void UTowedActorComponent::ClearTowRelationship()
{
	M_TowingActor = nullptr;
	M_TowingVehicleTowComp = nullptr;
}

AActor* UTowedActorComponent::GetTowingActor() const
{
	if (M_TowingActor.IsValid())
	{
		return M_TowingActor.Get();
	}

	return nullptr;
}

UVehicleTowComponent* UTowedActorComponent::GetTowingVehicleTowComp() const
{
	if (M_TowingVehicleTowComp.IsValid())
	{
		return M_TowingVehicleTowComp.Get();
	}

	return nullptr;
}

void UTowedActorComponent::RemoveAbilitiesWhileTowed()
{
	ICommands* CommandsOwner = GetOwningICommands();
	if (not CommandsOwner)
	{
		RTSFunctionLibrary::ReportError("The towed actor component could not find a valid icommands interface"
			"on its owner nor the TeamWeaponController of the owner!");
		return;
	}
	UCommandData* CommandData = CommandsOwner->GetIsValidCommandData();
	if (not IsValid(CommandData))
	{
		return;
	}

	M_CachedRemovedAbilities.Reset();
	const TArray<FUnitAbilityEntry> CurrentAbilities = CommandData->GetAbilities();
	for (const EAbilityID AbilityToRemove : M_TowedSettings.AbilitiesToRemoveWhileTowed)
	{
		for (int32 AbilityIndex = 0; AbilityIndex < CurrentAbilities.Num(); ++AbilityIndex)
		{
			if (CurrentAbilities[AbilityIndex].AbilityId != AbilityToRemove)
			{
				continue;
			}

			FTowedActorRemovedAbilityCache CacheEntry;
			CacheEntry.M_AbilityIndex = AbilityIndex;
			CacheEntry.M_AbilityEntry = CurrentAbilities[AbilityIndex];
			M_CachedRemovedAbilities.Add(CacheEntry);
			CommandData->RemoveAbility(AbilityToRemove);
			break;
		}
	}
}

void UTowedActorComponent::RestoreAbilitiesAfterTow()
{
	ICommands* CommandsOwner = GetOwningICommands();
	if (not CommandsOwner)
	{
		RTSFunctionLibrary::ReportError("The towed actor component could not find a valid icommands interface"
			"on its owner nor the TeamWeaponController of the owner!");
		return;
	}
	UCommandData* CommandData = CommandsOwner->GetIsValidCommandData();
	if (not IsValid(CommandData))
	{
		return;
	}

	for (const FTowedActorRemovedAbilityCache& CachedAbility : M_CachedRemovedAbilities)
	{
		CommandData->AddAbility(CachedAbility.M_AbilityEntry, CachedAbility.M_AbilityIndex);
	}

	M_CachedRemovedAbilities.Reset();
}


ICommands* UTowedActorComponent::GetOwningICommands() const
{
	if (not IsValid(GetOwner()))
	{
		return nullptr;
	}
	if (GetOwner()->IsA(ATeamWeaponController::StaticClass()))
	{
		ATeamWeaponController* TeamWeaponController = Cast<ATeamWeaponController>(GetOwner());
		return TeamWeaponController;
	}
	ICommands* CommandsOwner = Cast<ICommands>(GetOwner());
	return CommandsOwner;
}
