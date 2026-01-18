// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehWeaponRemoveAbilities.h"

#include "RTS_Survival/Interfaces/Commands.h"

UBehWeaponRemoveAbilities::UBehWeaponRemoveAbilities()
{
	BehaviourStackRule = EBehaviourStackRule::Exclusive;
	M_MaxStackCount = 1;
}

void UBehWeaponRemoveAbilities::OnAdded(AActor* BehaviourOwner)
{
	ICommands* CommandsInterface = nullptr;
	if (BehaviourOwner != nullptr)
	{
		CommandsInterface = Cast<ICommands>(BehaviourOwner);
	}

	if (CommandsInterface != nullptr)
	{
		CommandsInterface->SetUnitToIdle();
		CacheRemovedAbilities(CommandsInterface);
	}

	// Make sure to call the bp event.
	Super::OnAdded(BehaviourOwner);
}

void UBehWeaponRemoveAbilities::OnRemoved(AActor* BehaviourOwner)
{
	ICommands* CommandsInterface = nullptr;
	if (BehaviourOwner != nullptr)
	{
		CommandsInterface = Cast<ICommands>(BehaviourOwner);
	}

	if (CommandsInterface != nullptr)
	{
		RestoreRemovedAbilities(CommandsInterface);
	}

	// Make sure to call the bp event.
	Super::OnRemoved(BehaviourOwner);
}

void UBehWeaponRemoveAbilities::CacheRemovedAbilities(ICommands* CommandsInterface)
{
	if (CommandsInterface == nullptr)
	{
		return;
	}

	M_RemovedAbilityEntries.Reset();

	const TArray<FUnitAbilityEntry> AbilityEntries = CommandsInterface->GetUnitAbilityEntries();
	TSet<EAbilityID> RemovedAbilityIds;

	for (const EAbilityID AbilityToRemove : M_AbilitiesToRemove)
	{
		if (AbilityToRemove == EAbilityID::IdNoAbility)
		{
			continue;
		}

		if (RemovedAbilityIds.Contains(AbilityToRemove))
		{
			continue;
		}

		const int32 AbilityIndex = AbilityEntries.IndexOfByPredicate([AbilityToRemove](
			const FUnitAbilityEntry& AbilityEntry)
		{
			return AbilityEntry.AbilityId == AbilityToRemove;
		});

		if (AbilityIndex == INDEX_NONE)
		{
			continue;
		}

		const FUnitAbilityEntry AbilityEntry = AbilityEntries[AbilityIndex];
		if (AbilityEntry.AbilityId == EAbilityID::IdNoAbility)
		{
			continue;
		}

		if (not CommandsInterface->RemoveAbility(AbilityEntry.AbilityId))
		{
			continue;
		}

		FBehWeaponRemovedAbility RemovedAbility;
		RemovedAbility.AbilityEntry = AbilityEntry;
		RemovedAbility.AbilityIndex = AbilityIndex;
		M_RemovedAbilityEntries.Add(RemovedAbility);
		RemovedAbilityIds.Add(AbilityToRemove);
	}
}

void UBehWeaponRemoveAbilities::RestoreRemovedAbilities(ICommands* CommandsInterface)
{
	if (CommandsInterface == nullptr)
	{
		return;
	}

	for (const FBehWeaponRemovedAbility& RemovedAbility : M_RemovedAbilityEntries)
	{
		if (RemovedAbility.AbilityEntry.AbilityId == EAbilityID::IdNoAbility)
		{
			continue;
		}

		CommandsInterface->AddAbility(RemovedAbility.AbilityEntry, RemovedAbility.AbilityIndex);
	}

	M_RemovedAbilityEntries.Reset();
}
