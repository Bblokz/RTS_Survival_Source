// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TankMaster.h"

#include "AITankMaster.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"

using namespace VehicleModuleBalance;

namespace TankMasterCrewRepairConstants
{
	// Absorbs timer jitter so a module completes on the tick that reaches its full duration.
	constexpr double ElapsedToleranceSeconds = 0.01;
}

// ----------------------------------------------------------------------------------------------------
// Command card
// ----------------------------------------------------------------------------------------------------

void ATankMaster::InitializeCrewRepairAbilitySlot()
{
	UCommandData* CrewCommandData = GetIsValidCommandData();
	if (not IsValid(CrewCommandData))
	{
		return;
	}
	// The final slot is reserved once; later ability rebuilds preserve it.
	if (not CrewCommandData->ReserveCrewRepairAbilitySlot())
	{
		return;
	}
	RefreshCrewRepairAbilityFromModuleState();
}

FUnitAbilityEntry ATankMaster::MakeCrewRepairAbilityEntry(const ECrewRepairAbilityType Subtype) const
{
	FUnitAbilityEntry CrewRepairEntry;
	CrewRepairEntry.AbilityId = EAbilityID::IdCrewRepair;
	CrewRepairEntry.CustomType = static_cast<int32>(Subtype);
	CrewRepairEntry.CooldownDuration = CrewRepairCooldownSeconds;
	return CrewRepairEntry;
}

void ATankMaster::RefreshCrewRepairAbilityFromModuleState()
{
	UCommandData* CrewCommandData = GetIsValidCommandData();
	if (not IsValid(CrewCommandData) || not CrewCommandData->GetIsCrewRepairAbilitySlotReserved())
	{
		return;
	}

	// Red modules: EnableRepair while inactive, DisableRepair while repairing; no red modules: empty slot.
	FUnitAbilityEntry DesiredEntry;
	const bool bHasRedModules = IsUnitAlive() && M_ModuleRepairState.DestroyedModuleCount > 0;
	if (bHasRedModules)
	{
		const bool bIsRepairing = M_CrewRepairState.Status == ECrewRepairStatus::Repairing;
		DesiredEntry = MakeCrewRepairAbilityEntry(bIsRepairing
			                                          ? ECrewRepairAbilityType::DisableRepair
			                                          : ECrewRepairAbilityType::EnableRepair);
	}
	// Writes and refreshes the card only when the desired entry differs.
	if (CrewCommandData->SetCrewRepairAbilityEntry(DesiredEntry))
	{
		CrewCommandData->UpdateActionUI();
	}
}

// ----------------------------------------------------------------------------------------------------
// Execute / terminate
// ----------------------------------------------------------------------------------------------------

bool ATankMaster::GetIsCrewRepairActive() const
{
	return M_CrewRepairState.Status == ECrewRepairStatus::Repairing;
}

void ATankMaster::ExecuteCrewRepairCommand(const ECrewRepairAbilityType Subtype)
{
	if (Subtype == ECrewRepairAbilityType::DisableRepair)
	{
		// A stale Disable after automatic completion is a harmless no-op.
		FinishCrewRepair(ECrewRepairStopReason::PlayerDisabled);
		return;
	}
	if (M_CrewRepairState.Status != ECrewRepairStatus::Inactive)
	{
		return;
	}
	if (BeginCrewRepair())
	{
		return;
	}
	// Start failed after rollback: complete this queued Enable command once so the queue continues.
	if (GetCurrentActiveCommand() == EAbilityID::IdCrewRepair)
	{
		DoneExecutingCommand(EAbilityID::IdCrewRepair);
	}
}

void ATankMaster::TerminateCrewRepairCommand(const ECrewRepairAbilityType Subtype)
{
	// The queue owns progression here, so cleanup never calls DoneExecutingCommand again.
	FinishCrewRepair(ECrewRepairStopReason::QueueTermination);
}

bool ATankMaster::GetShouldIgnoreCommandCompletion(const EAbilityID AbilityFinished)
{
	// Stale completions (e.g. an aborted move task) must not complete the running crew repair command.
	return M_CrewRepairState.Status == ECrewRepairStatus::Repairing
		&& AbilityFinished != EAbilityID::IdCrewRepair
		&& GetCurrentActiveCommand() == EAbilityID::IdCrewRepair;
}

// ----------------------------------------------------------------------------------------------------
// Session lifecycle
// ----------------------------------------------------------------------------------------------------

bool ATankMaster::BeginCrewRepair()
{
	if (M_CrewRepairState.Status != ECrewRepairStatus::Inactive || not IsUnitAlive() || not bM_AreVehicleModulesInitialized)
	{
		return false;
	}
	UCommandData* CrewCommandData = GetIsValidCommandData();
	if (not IsValid(CrewCommandData) || not CrewCommandData->GetIsCrewRepairAbilitySlotReserved()
		|| not GetIsValidModuleArmor() || M_ModuleArmor->GetDestroyedModuleCount() <= 0)
	{
		return false;
	}
	if (not SelectNextRedModuleForCrewRepair())
	{
		return false;
	}

	++M_CrewRepairState.SessionGeneration;
	M_CrewRepairState.Status = ECrewRepairStatus::Repairing;
	const bool bOwnsQueuedCommand = CrewCommandData->GetCurrentActiveCommand() == EAbilityID::IdCrewRepair;
	M_CrewRepairState.ActiveCommandToken = bOwnsQueuedCommand ? CrewCommandData->GetCurrentCommandExecutionSerial() : 0;
	AcquireCrewRepairRestrictions(*CrewCommandData);
	if (not StartCrewRepairTimer())
	{
		// Any start failure rolls back locks, abilities, card and timer.
		FinishCrewRepair(ECrewRepairStopReason::StartFailed);
		return false;
	}
	RefreshCrewRepairAbilityFromModuleState();
	CrewCommandData->UpdateActionUI();
	return true;
}

void ATankMaster::AcquireCrewRepairRestrictions(UCommandData& CommandData)
{
	// Restrictions come from the explicitly activated repair action, never from red module state alone.
	M_CrewRepairState.bHoldsWeaponAndMovementLock = true;
	AcquireMountedWeaponLock(this);
	StopVehicleForCrewRepair();
	M_CrewRepairState.AbilitySuppressionHandle = CommandData.BeginAbilitySuppression(
		this, MakeArrayView(CrewRepairSuppressedAbilities));
}

void ATankMaster::StopVehicleForCrewRepair()
{
	// ExecuteStopCommand only re-enables auto-engage; actual movement and rotation are stopped here.
	EndTurretRangeMovement(ETurretRangeMovementEndMode::FullStop);
	StopBehaviourTree();
	if (GetIsValidAIController())
	{
		AITankController->StopMovement();
	}
	StopRotating();
	ResetRotateTowardsFinalMovementRotation();
	StopMovement();
}

bool ATankMaster::StartCrewRepairTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	// UObject-bound repeating timer; the payload generation rejects callbacks from a previous session.
	const FTimerDelegate TickDelegate = FTimerDelegate::CreateUObject(
		this, &ATankMaster::CrewRepairTick, M_CrewRepairState.SessionGeneration);
	World->GetTimerManager().SetTimer(M_CrewRepairState.TimerHandle, TickDelegate, CrewRepairTickSeconds, true,
	                                  CrewRepairTickSeconds);
	return true;
}

void ATankMaster::CrewRepairTick(const uint32 SessionGeneration)
{
	if (SessionGeneration != M_CrewRepairState.SessionGeneration
		|| M_CrewRepairState.Status != ECrewRepairStatus::Repairing)
	{
		return;
	}
	if (not IsUnitAlive() || not GetIsValidModuleArmor())
	{
		FinishCrewRepair(ECrewRepairStopReason::OwnerDestroyed);
		return;
	}
	if (M_ModuleArmor->GetDestroyedModuleCount() <= 0)
	{
		FinishCrewRepair(ECrewRepairStopReason::AllModulesRecovered);
		return;
	}
	const FVehicleModuleSnapshot Target = M_ModuleArmor->GetModuleSnapshot(M_CrewRepairState.CurrentModuleId);
	if (Target.State != EVehicleModuleState::Destroyed)
	{
		// The previous target left red state elsewhere; the new target starts with its full duration.
		(void)SelectNextRedModuleForCrewRepair();
		return;
	}

	const double ElapsedSeconds = GetWorld()->GetTimeSeconds() - M_CrewRepairState.ModuleWorkStartGameTime;
	if (ElapsedSeconds + TankMasterCrewRepairConstants::ElapsedToleranceSeconds < M_CrewRepairState.RequiredModuleSeconds)
	{
		return;
	}
	// The committed state batch advances the target or auto-finishes; nothing else to do here.
	M_ModuleArmor->RestoreDestroyedModuleToDamaged(M_CrewRepairState.CurrentModuleId);
}

bool ATankMaster::SelectNextRedModuleForCrewRepair()
{
	const UWorld* World = GetWorld();
	if (not GetIsValidModuleArmor() || not IsValid(World))
	{
		return false;
	}
	const int32 NextModuleId = M_ModuleArmor->SelectNextRedModuleForCrewRepair();
	M_CrewRepairState.CurrentModuleId = NextModuleId;
	if (NextModuleId == INDEX_NONE)
	{
		return false;
	}
	// Unfinished time of a previous target is discarded; a target selected between ticks never completes early.
	const EVehicleModuleTypes TargetType = M_ModuleArmor->GetModuleSnapshot(NextModuleId).Type;
	M_CrewRepairState.RequiredModuleSeconds = GetCrewRepairSeconds(TargetType);
	M_CrewRepairState.ModuleWorkStartGameTime = World->GetTimeSeconds();
	return true;
}

void ATankMaster::UpdateCrewRepairAfterModuleBatch()
{
	if (M_CrewRepairState.Status != ECrewRepairStatus::Repairing || not GetIsValidModuleArmor())
	{
		return;
	}
	// Another healer may have restored every red module first; undo only the crew's restrictions.
	if (M_ModuleRepairState.DestroyedModuleCount <= 0)
	{
		FinishCrewRepair(ECrewRepairStopReason::AllModulesRecovered);
		return;
	}
	const FVehicleModuleSnapshot Target = M_ModuleArmor->GetModuleSnapshot(M_CrewRepairState.CurrentModuleId);
	if (Target.State == EVehicleModuleState::Destroyed)
	{
		// Newly red modules join the remaining work without resetting the current target.
		return;
	}
	if (not SelectNextRedModuleForCrewRepair())
	{
		FinishCrewRepair(ECrewRepairStopReason::AllModulesRecovered);
	}
}

void ATankMaster::FinishCrewRepair(const ECrewRepairStopReason Reason)
{
	if (M_CrewRepairState.Status != ECrewRepairStatus::Repairing)
	{
		return;
	}
	M_CrewRepairState.Status = ECrewRepairStatus::Stopping;
	++M_CrewRepairState.SessionGeneration;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CrewRepairState.TimerHandle);
	}

	RestoreCrewRepairState();
	// Unfinished work on the current target is lost; completed yellow repairs remain.
	M_CrewRepairState.CurrentModuleId = INDEX_NONE;
	M_CrewRepairState.ModuleWorkStartGameTime = 0.0;
	M_CrewRepairState.RequiredModuleSeconds = 0.f;
	const uint64 CommandToken = M_CrewRepairState.ActiveCommandToken;
	M_CrewRepairState.ActiveCommandToken = 0;
	M_CrewRepairState.Status = ECrewRepairStatus::Inactive;
	if (Reason == ECrewRepairStopReason::OwnerDestroyed)
	{
		return;
	}

	RefreshCrewRepairAbilityFromModuleState();
	if (UCommandData* CrewCommandData = GetIsValidCommandData())
	{
		CrewCommandData->UpdateActionUI();
	}
	// Complete the owned queued Enable exactly once, after cleanup; queue termination already progresses.
	const bool bShouldCompleteCommand = Reason != ECrewRepairStopReason::QueueTermination && CommandToken != 0;
	if (bShouldCompleteCommand)
	{
		(void)TryDoneExecutingCommand(EAbilityID::IdCrewRepair, CommandToken);
	}
}

void ATankMaster::RestoreCrewRepairState()
{
	// Release only crew-owned suppression; entries hidden by other sources stay hidden.
	UCommandData* CrewCommandData = GetIsValidCommandData();
	if (IsValid(CrewCommandData))
	{
		CrewCommandData->EndAbilitySuppression(M_CrewRepairState.AbilitySuppressionHandle);
	}
	M_CrewRepairState.AbilitySuppressionHandle.Reset();

	if (not M_CrewRepairState.bHoldsWeaponAndMovementLock)
	{
		return;
	}
	M_CrewRepairState.bHoldsWeaponAndMovementLock = false;
	// On death the dead-unit check inside the release keeps weapons disabled.
	constexpr bool bUseLastTargetOnRestore = false;
	ReleaseMountedWeaponLock(this, bUseLastTargetOnRestore);
}

float ATankMaster::GetRemainingCrewRepairSeconds() const
{
	if (not M_ModuleArmor.IsValid())
	{
		return 0.f;
	}
	float RemainingSeconds = 0.f;
	for (int32 SlotIndex = 0; SlotIndex < MaxModuleInstances; ++SlotIndex)
	{
		const FVehicleModuleSnapshot Snapshot = M_ModuleArmor->GetModuleSnapshotForSlot(SlotIndex);
		if (Snapshot.bInstalled && Snapshot.State == EVehicleModuleState::Destroyed)
		{
			RemainingSeconds += GetCrewRepairSeconds(Snapshot.Type);
		}
	}
	const UWorld* World = GetWorld();
	if (M_CrewRepairState.Status == ECrewRepairStatus::Repairing && IsValid(World))
	{
		const double ElapsedSeconds = World->GetTimeSeconds() - M_CrewRepairState.ModuleWorkStartGameTime;
		RemainingSeconds -= FMath::Clamp(static_cast<float>(ElapsedSeconds), 0.f, M_CrewRepairState.RequiredModuleSeconds);
	}
	return FMath::Max(0.f, RemainingSeconds);
}
