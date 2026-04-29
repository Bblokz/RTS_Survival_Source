// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehVehicleStunned.h"

#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

namespace BehVehicleStunnedConstants
{
	constexpr int32 MaximumStackCount = 1;
}

UBehVehicleStunned::UBehVehicleStunned()
{
	BehaviourStackRule = EBehaviourStackRule::Exclusive;
	M_MaxStackCount = BehVehicleStunnedConstants::MaximumStackCount;

	M_AbilitiesToRemove =
	{
		EAbilityID::IdMove,
		EAbilityID::IdReverseMove
	};
}

void UBehVehicleStunned::OnAdded(AActor* BehaviourOwner)
{
	CacheOwnerReferences(BehaviourOwner);

	SetOwnerToIdleAndStopMovement();
	DisableMountedWeapons();
	ApplyTurretRotationPenalty();
	CacheAndRemoveAbilities();

	// Make sure to call the bp event.
	Super::OnAdded(BehaviourOwner);
}

void UBehVehicleStunned::OnRemoved(AActor* BehaviourOwner)
{
	RestoreTurretRotation();
	EnableMountedWeapons();
	RestoreRemovedAbilities();
	ResetCachedState();

	// Make sure to call the bp event.
	Super::OnRemoved(BehaviourOwner);
}

void UBehVehicleStunned::CacheOwnerReferences(AActor* BehaviourOwner)
{
	M_TankMaster = Cast<ATankMaster>(BehaviourOwner);
	M_CommandsOwnerActor = BehaviourOwner;

	(void)GetIsValidTankMaster();
	(void)GetIsValidCommandsOwnerActor();
}

void UBehVehicleStunned::SetOwnerToIdleAndStopMovement() const
{
	if (not GetIsValidCommandsOwnerActor())
	{
		return;
	}

	ICommands* CommandsInterface = GetCommandsInterface();
	if (CommandsInterface == nullptr)
	{
		RTSFunctionLibrary::ReportFailedCastError(
			TEXT("M_CommandsOwnerActor"),
			TEXT("ICommands"),
			TEXT("UBehVehicleStunned::SetOwnerToIdleAndStopMovement")
		);
		return;
	}

	CommandsInterface->SetUnitToIdle();
	CommandsInterface->StopMovement();
}

void UBehVehicleStunned::ApplyTurretRotationPenalty()
{
	if (not GetIsValidTankMaster())
	{
		return;
	}

	const TArray<ACPPTurretsMaster*> Turrets = M_TankMaster->GetTurrets();
	for (ACPPTurretsMaster* Turret : Turrets)
	{
		if (Turret == nullptr)
		{
			continue;
		}

		const TWeakObjectPtr<ACPPTurretsMaster> TurretPtr = Turret;
		if (M_CachedTurretRotationSpeeds.Contains(TurretPtr))
		{
			continue;
		}

		const float BaseRotationSpeed = Turret->GetTurretRotationSpeed();
		M_CachedTurretRotationSpeeds.Add(TurretPtr, BaseRotationSpeed);
		Turret->SetTurretRotationSpeed(BaseRotationSpeed * M_TurretRotationSpeedMultiplier);
	}
}

void UBehVehicleStunned::RestoreTurretRotation()
{
	for (const TPair<TWeakObjectPtr<ACPPTurretsMaster>, float>& CachedTurretSpeed : M_CachedTurretRotationSpeeds)
	{
		if (not CachedTurretSpeed.Key.IsValid())
		{
			continue;
		}

		CachedTurretSpeed.Key->SetTurretRotationSpeed(CachedTurretSpeed.Value);
	}

	M_CachedTurretRotationSpeeds.Reset();
}

void UBehVehicleStunned::DisableMountedWeapons() const
{
	if (not GetIsValidTankMaster())
	{
		return;
	}

	M_TankMaster->SetTurretsDisabled();
}

void UBehVehicleStunned::EnableMountedWeapons() const
{
	if (not GetIsValidTankMaster())
	{
		return;
	}

	M_TankMaster->SetTurretsToAutoEngage(true);
}

void UBehVehicleStunned::CacheAndRemoveAbilities()
{
	if (not GetIsValidCommandsOwnerActor())
	{
		return;
	}

	ICommands* CommandsInterface = GetCommandsInterface();
	if (CommandsInterface == nullptr)
	{
		RTSFunctionLibrary::ReportFailedCastError(
			TEXT("M_CommandsOwnerActor"),
			TEXT("ICommands"),
			TEXT("UBehVehicleStunned::CacheAndRemoveAbilities")
		);
		return;
	}

	M_RemovedAbilityEntries.Reset();

	const TArray<FUnitAbilityEntry> AbilityEntries = CommandsInterface->GetUnitAbilityEntries();
	TSet<EAbilityID> RemovedAbilityIDs;

	for (const EAbilityID AbilityToRemove : M_AbilitiesToRemove)
	{
		if (AbilityToRemove == EAbilityID::IdNoAbility)
		{
			continue;
		}

		if (RemovedAbilityIDs.Contains(AbilityToRemove))
		{
			continue;
		}

		const int32 AbilityIndex = AbilityEntries.IndexOfByPredicate([AbilityToRemove](const FUnitAbilityEntry& AbilityEntry)
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

		FBehVehicleStunnedRemovedAbility RemovedAbility;
		RemovedAbility.AbilityEntry = AbilityEntry;
		RemovedAbility.AbilityIndex = AbilityIndex;
		M_RemovedAbilityEntries.Add(RemovedAbility);
		RemovedAbilityIDs.Add(AbilityToRemove);
	}
}

void UBehVehicleStunned::RestoreRemovedAbilities()
{
	if (not GetIsValidCommandsOwnerActor())
	{
		M_RemovedAbilityEntries.Reset();
		return;
	}

	ICommands* CommandsInterface = GetCommandsInterface();
	if (CommandsInterface == nullptr)
	{
		RTSFunctionLibrary::ReportFailedCastError(
			TEXT("M_CommandsOwnerActor"),
			TEXT("ICommands"),
			TEXT("UBehVehicleStunned::RestoreRemovedAbilities")
		);
		M_RemovedAbilityEntries.Reset();
		return;
	}

	for (const FBehVehicleStunnedRemovedAbility& RemovedAbility : M_RemovedAbilityEntries)
	{
		if (RemovedAbility.AbilityEntry.AbilityId == EAbilityID::IdNoAbility)
		{
			continue;
		}

		CommandsInterface->AddAbility(RemovedAbility.AbilityEntry, RemovedAbility.AbilityIndex);
	}

	M_RemovedAbilityEntries.Reset();
}

void UBehVehicleStunned::ResetCachedState()
{
	M_CachedTurretRotationSpeeds.Reset();
	M_RemovedAbilityEntries.Reset();
	M_TankMaster.Reset();
	M_CommandsOwnerActor.Reset();
}

ICommands* UBehVehicleStunned::GetCommandsInterface() const
{
	if (not GetIsValidCommandsOwnerActor())
	{
		return nullptr;
	}

	return Cast<ICommands>(M_CommandsOwnerActor.Get());
}

bool UBehVehicleStunned::GetIsValidTankMaster() const
{
	if (M_TankMaster.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_TankMaster"),
		TEXT("GetIsValidTankMaster"),
		this
	);

	return false;
}

bool UBehVehicleStunned::GetIsValidCommandsOwnerActor() const
{
	if (M_CommandsOwnerActor.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_CommandsOwnerActor"),
		TEXT("GetIsValidCommandsOwnerActor"),
		this
	);

	return false;
}
