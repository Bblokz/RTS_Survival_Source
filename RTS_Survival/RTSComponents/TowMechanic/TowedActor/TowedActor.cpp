#include "TowedActor.h"

#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/TowMechanic/VehicleTow/VehicleTow.h"
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
	if (not GetIsValidCommandsOwner())
	{
		return;
	}

	ICommands* CommandsOwner = Cast<ICommands>(GetOwner());
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
	if (not GetIsValidCommandsOwner())
	{
		return;
	}

	ICommands* CommandsOwner = Cast<ICommands>(GetOwner());
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

bool UTowedActorComponent::GetIsValidCommandsOwner() const
{
	if (IsValid(Cast<ICommands>(GetOwner())))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("UTowedActorComponent owner does not implement ICommands in "
		"UTowedActorComponent::GetIsValidCommandsOwner");
	return false;
}
