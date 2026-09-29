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

	UCommandData* CommandData = CommandsInterface->GetIsValidCommandData();
	if (not IsValid(CommandData))
	{
		return;
	}

	// Shares suppression ownership with crew repair and stuns so overlapping removals restore correctly.
	CommandData->EndAbilitySuppression(M_AbilitySuppressionHandle);
	M_AbilitySuppressionHandle = CommandData->BeginAbilitySuppression(this, M_AbilitiesToRemove);
	CommandData->UpdateActionUI();
}

void UBehWeaponRemoveAbilities::RestoreRemovedAbilities(ICommands* CommandsInterface)
{
	if (CommandsInterface == nullptr)
	{
		M_AbilitySuppressionHandle.Reset();
		return;
	}

	UCommandData* CommandData = CommandsInterface->GetIsValidCommandData();
	if (not IsValid(CommandData))
	{
		M_AbilitySuppressionHandle.Reset();
		return;
	}

	CommandData->EndAbilitySuppression(M_AbilitySuppressionHandle);
	CommandData->UpdateActionUI();
}
