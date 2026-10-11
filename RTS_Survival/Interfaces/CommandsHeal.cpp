// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Commands.h"

#include "RTS_Survival/RTSComponents/MedicComponent/HealAbilityTypes.h"

// ----------------------------------------------------------------------------------------------------
// ICommands Heal API
// ----------------------------------------------------------------------------------------------------

ECommandQueueError ICommands::HealTarget(AActor* TargetActor, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not IsValid(TargetActor))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	// Either card subtype will do: while a heal order runs the card shows the cancel entry, and a heal order
	// for another squad may still replace the running one or be queued behind it.
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdHeal);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	return UnitCommandData->AddAbilityToTCommands(EAbilityID::IdHeal, FVector::ZeroVector, TargetActor,
	                                              FRotator::ZeroRotator);
}

ECommandQueueError ICommands::CancelHeal()
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	const int32 CancelSubtype = static_cast<int32>(EHealAbilityType::HealingDisabled);
	if (UnitCommandData->GetAbilityEntryOfCustomType(EAbilityID::IdHeal, CancelSubtype) == nullptr)
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	// Immediate control action: never queued behind the heal order it cancels.
	ExecuteCancelHealCommand();
	return ECommandQueueError::NoError;
}

void ICommands::ExecuteHealCommand(AActor* TargetActor)
{
	// Unsupported owners reject safely without stalling the queue.
	DoneExecutingCommand(EAbilityID::IdHeal);
}

void ICommands::ExecuteCancelHealCommand()
{
}

void ICommands::TerminateHealCommand()
{
}
