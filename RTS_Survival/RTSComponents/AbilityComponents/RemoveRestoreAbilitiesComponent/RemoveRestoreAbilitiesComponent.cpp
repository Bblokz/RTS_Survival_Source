// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RemoveRestoreAbilitiesComponent.h"

#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

URemoveRestoreAbilitiesComponent::URemoveRestoreAbilitiesComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URemoveRestoreAbilitiesComponent::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_InitOwnerCommandsInterface();
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}

	BeginPlay_RemoveConfiguredAbilities();
}

void URemoveRestoreAbilitiesComponent::RestoreAbility(const EAbilityID AbilityIdToRestore)
{
	if (AbilityIdToRestore == EAbilityID::IdNoAbility)
	{
		RTSFunctionLibrary::ReportError("RestoreAbility called with IdNoAbility on component: " + GetName());
		return;
	}

	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}

	const int32 CachedAbilityIndex = GetCachedAbilityIndex(AbilityIdToRestore);
	if (CachedAbilityIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(
			"RestoreAbility could not find cached ability: " + Global_GetAbilityIDAsString(AbilityIdToRestore)
			+ " on owner: " + GetOwner()->GetName()
		);
		return;
	}

	if (not RestoreCachedAbilityByCacheIndex(CachedAbilityIndex))
	{
		return;
	}

	M_CachedRemovedAbilities.RemoveAt(CachedAbilityIndex);
}

void URemoveRestoreAbilitiesComponent::RestoreAllCachedAbilities()
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}

	for (int32 CachedAbilityIndex = M_CachedRemovedAbilities.Num() - 1; CachedAbilityIndex >= 0; --CachedAbilityIndex)
	{
		if (not RestoreCachedAbilityByCacheIndex(CachedAbilityIndex))
		{
			continue;
		}

		M_CachedRemovedAbilities.RemoveAt(CachedAbilityIndex);
	}
}

bool URemoveRestoreAbilitiesComponent::GetIsValidOwnerCommandsInterface() const
{
	if (IsValid(M_OwnerCommandsInterface.GetObject()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_OwnerCommandsInterface",
		"GetIsValidOwnerCommandsInterface",
		GetOwner()
	);
	return false;
}

void URemoveRestoreAbilitiesComponent::BeginPlay_InitOwnerCommandsInterface()
{
	ICommands* CommandsInterface = Cast<ICommands>(GetOwner());
	if (not CommandsInterface)
	{
		RTSFunctionLibrary::ReportError(
			"RemoveRestoreAbilitiesComponent owner does not implement ICommands: " + GetOwner()->GetName()
		);
		return;
	}

	M_OwnerCommandsInterface.SetInterface(CommandsInterface);
	M_OwnerCommandsInterface.SetObject(GetOwner());
}

void URemoveRestoreAbilitiesComponent::BeginPlay_RemoveConfiguredAbilities()
{
	for (const EAbilityID AbilityToRemove : M_AbilitiesToRemoveOnBeginPlay)
	{
		(void)RemoveAndCacheAbility(AbilityToRemove);
	}
}

bool URemoveRestoreAbilitiesComponent::RemoveAndCacheAbility(const EAbilityID AbilityToRemove)
{
	if (AbilityToRemove == EAbilityID::IdNoAbility)
	{
		RTSFunctionLibrary::ReportError(
			"AbilitiesToRemoveOnBeginPlay contains IdNoAbility on owner: " + GetOwner()->GetName()
		);
		return false;
	}

	if (GetCachedAbilityIndex(AbilityToRemove) != INDEX_NONE)
	{
		return true;
	}

	FUnitAbilityEntry AbilityEntryToRemove;
	int32 AbilityIndexToRemove = INDEX_NONE;
	if (not TryGetOwnerAbilityEntry(AbilityToRemove, AbilityEntryToRemove, AbilityIndexToRemove))
	{
		RTSFunctionLibrary::ReportError(
			"Could not remove ability that owner does not have: " + Global_GetAbilityIDAsString(AbilityToRemove)
			+ " owner: " + GetOwner()->GetName()
		);
		return false;
	}

	if (not M_OwnerCommandsInterface->RemoveAbility(AbilityToRemove))
	{
		RTSFunctionLibrary::ReportError(
			"Failed removing configured ability: " + Global_GetAbilityIDAsString(AbilityToRemove)
			+ " owner: " + GetOwner()->GetName()
		);
		return false;
	}

	FRemoveRestoreCachedAbility CachedAbility;
	CachedAbility.AbilityEntry = AbilityEntryToRemove;
	CachedAbility.RemovedFromIndex = AbilityIndexToRemove;
	M_CachedRemovedAbilities.Add(CachedAbility);
	return true;
}

bool URemoveRestoreAbilitiesComponent::TryGetOwnerAbilityEntry(const EAbilityID AbilityToFind,
	FUnitAbilityEntry& OutAbilityEntry, int32& OutAbilityIndex) const
{
	const TArray<FUnitAbilityEntry> OwnerAbilityEntries = M_OwnerCommandsInterface->GetUnitAbilityEntries();
	const int32 FoundAbilityIndex = OwnerAbilityEntries.IndexOfByPredicate([AbilityToFind](const FUnitAbilityEntry& AbilityEntry)
	{
		return AbilityEntry.AbilityId == AbilityToFind;
	});

	if (not OwnerAbilityEntries.IsValidIndex(FoundAbilityIndex))
	{
		return false;
	}

	OutAbilityIndex = FoundAbilityIndex;
	OutAbilityEntry = OwnerAbilityEntries[FoundAbilityIndex];
	return true;
}

int32 URemoveRestoreAbilitiesComponent::GetCachedAbilityIndex(const EAbilityID AbilityIdToFind) const
{
	return M_CachedRemovedAbilities.IndexOfByPredicate([AbilityIdToFind](const FRemoveRestoreCachedAbility& CachedAbility)
	{
		return CachedAbility.AbilityEntry.AbilityId == AbilityIdToFind;
	});
}

bool URemoveRestoreAbilitiesComponent::RestoreCachedAbilityByCacheIndex(const int32 CachedAbilityIndex)
{
	if (not M_CachedRemovedAbilities.IsValidIndex(CachedAbilityIndex))
	{
		RTSFunctionLibrary::ReportError(
			"RestoreCachedAbilityByCacheIndex failed with invalid index: " + FString::FromInt(CachedAbilityIndex)
			+ " owner: " + GetOwner()->GetName()
		);
		return false;
	}

	const FRemoveRestoreCachedAbility& CachedAbility = M_CachedRemovedAbilities[CachedAbilityIndex];
	if (CachedAbility.AbilityEntry.AbilityId == EAbilityID::IdNoAbility)
	{
		RTSFunctionLibrary::ReportError(
			"Cached ability was invalid (IdNoAbility) on owner: " + GetOwner()->GetName()
		);
		return false;
	}

	if (not M_OwnerCommandsInterface->AddAbility(CachedAbility.AbilityEntry, CachedAbility.RemovedFromIndex))
	{
		const FString AbilityName = Global_GetAbilityIDAsString(CachedAbility.AbilityEntry.AbilityId);
		RTSFunctionLibrary::ReportError(
			"Failed to restore cached ability: " + AbilityName +
			" subtype: " + FString::FromInt(CachedAbility.AbilityEntry.CustomType) +
			" owner: " + GetOwner()->GetName()
		);
		return false;
	}

	return true;
}
