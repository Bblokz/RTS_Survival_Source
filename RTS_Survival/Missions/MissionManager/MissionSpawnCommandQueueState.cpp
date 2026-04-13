#include "MissionSpawnCommandQueueState.h"

#include "MissionManager.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/PickupItems/Items/ItemsMaster.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void FMissionSpawnCommandQueueState::Init(
	const int32 InRequestId,
	AMissionManager* InMissionManager,
	UMissionBase* InMissionOwner,
	const FTrainingOption& InTrainingOption,
	const int32 InSpawnId,
	const FVector& InSpawnLocation,
	const FRotator& InSpawnRotation,
	const TArray<TSubclassOf<UBehaviour>>& InBehavioursToApply,
	const TArray<FMissionSpawnCommandQueueOrder>& InCommandQueue)
{
	M_RequestId = InRequestId;
	M_MissionManager = InMissionManager;
	M_MissionOwner = InMissionOwner;
	M_TrainingOption = InTrainingOption;
	M_SpawnId = InSpawnId;
	M_SpawnLocation = InSpawnLocation;
	M_SpawnRotation = InSpawnRotation;
	M_BehavioursToApply = InBehavioursToApply;
	M_CommandQueue = InCommandQueue;
	M_SpawnedActor = nullptr;
	M_TicksSinceSpawn = 0;
	bM_HasSpawnCallbackExecuted = false;
	bM_HasFinished = false;
	bM_HasFailed = false;
}

bool FMissionSpawnCommandQueueState::StartAsyncSpawn(ARTSAsyncSpawner* RTSAsyncSpawner)
{
	if (not IsValid(RTSAsyncSpawner))
	{
		CompleteAsFailed("Mission spawn command queue failed to start: async spawner is invalid.");
		return false;
	}

	if (not GetIsValidMissionManager())
	{
		return false;
	}

	const TWeakObjectPtr<AMissionManager> WeakMissionManager = M_MissionManager;
	return RTSAsyncSpawner->AsyncSpawnOptionAtLocation(
		M_TrainingOption,
		M_SpawnLocation,
		M_MissionManager.Get(),
		M_SpawnId,
		[WeakMissionManager, RequestId = M_RequestId](const FTrainingOption&, AActor* SpawnedActor, const int32)->void
		{
			if (not WeakMissionManager.IsValid())
			{
				return;
			}
			WeakMissionManager->HandleSpawnActorWithCommandQueueSpawned(RequestId, SpawnedActor);
		});
}

bool FMissionSpawnCommandQueueState::HandleSpawnedActor(AActor* SpawnedActor)
{
	M_SpawnedActor = SpawnedActor;
	SetSpawnedActorRotation();
	NotifyMissionSpawnCompleted(SpawnedActor);

	if (not IsValid(SpawnedActor))
	{
		CompleteAsFailed("Mission spawn command queue received invalid spawned actor.");
		return false;
	}

	return true;
}

bool FMissionSpawnCommandQueueState::TickExecution()
{
	if (bM_HasFinished)
	{
		return true;
	}

	if (not GetIsValidSpawnedActor())
	{
		return false;
	}

	if (not GetCanExecuteQueueThisTick())
	{
		return false;
	}

	if (not ApplyQueuedBehaviours())
	{
		return false;
	}

	if (not ExecuteQueuedOrders())
	{
		return false;
	}

	bM_HasFinished = true;
	return true;
}

bool FMissionSpawnCommandQueueState::GetBelongsToRequest(const int32 RequestId) const
{
	return M_RequestId == RequestId;
}

bool FMissionSpawnCommandQueueState::GetIsFinished() const
{
	return bM_HasFinished;
}

bool FMissionSpawnCommandQueueState::GetIsValidMissionManager() const
{
	if (M_MissionManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Mission spawn command queue state has no valid mission manager.");
	return false;
}

bool FMissionSpawnCommandQueueState::GetIsValidMissionOwner() const
{
	if (M_MissionOwner.IsValid())
	{
		return true;
	}

	if (not GetIsValidMissionManager())
	{
		return false;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		M_MissionManager.Get(),
		"M_MissionOwner",
		"FMissionSpawnCommandQueueState::GetIsValidMissionOwner",
		M_MissionManager.Get());
	return false;
}

bool FMissionSpawnCommandQueueState::GetIsValidSpawnedActor() const
{
	if (M_SpawnedActor.IsValid())
	{
		return true;
	}

	if (bM_HasSpawnCallbackExecuted)
	{
		if (not GetIsValidMissionManager())
		{
			return false;
		}

		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			M_MissionManager.Get(),
			"M_SpawnedActor",
			"FMissionSpawnCommandQueueState::GetIsValidSpawnedActor",
			M_MissionManager.Get());
	}
	return false;
}

bool FMissionSpawnCommandQueueState::GetCanExecuteQueueThisTick()
{
	M_TicksSinceSpawn++;
	if (M_TicksSinceSpawn <= RequiredTicksBeforeQueueExecution)
	{
		return false;
	}

	ASquadController* SquadController = Cast<ASquadController>(M_SpawnedActor.Get());
	if (not IsValid(SquadController))
	{
		return true;
	}

	return GetCanExecuteQueueOnSquadController(SquadController);
}

bool FMissionSpawnCommandQueueState::GetCanExecuteQueueOnSquadController(ASquadController* SquadController) const
{
	if (not IsValid(SquadController))
	{
		return false;
	}

	return SquadController->GetIsSquadFullyLoaded();
}

void FMissionSpawnCommandQueueState::NotifyMissionSpawnCompleted(AActor* SpawnedActor)
{
	if (bM_HasSpawnCallbackExecuted)
	{
		return;
	}
	bM_HasSpawnCallbackExecuted = true;

	if (not GetIsValidMissionOwner())
	{
		return;
	}

	M_MissionOwner->BP_OnAsyncSpawnComplete(M_TrainingOption, SpawnedActor, M_SpawnId);
}

void FMissionSpawnCommandQueueState::SetSpawnedActorRotation()
{
	if (not M_SpawnedActor.IsValid())
	{
		return;
	}

	M_SpawnedActor->SetActorRotation(M_SpawnRotation, ETeleportType::ResetPhysics);
}

bool FMissionSpawnCommandQueueState::ExecuteQueuedOrders()
{
	if (M_CommandQueue.IsEmpty())
	{
		return true;
	}

	ICommands* CommandsInterface = Cast<ICommands>(M_SpawnedActor.Get());
	if (CommandsInterface == nullptr)
	{
		CompleteAsFailed("Mission spawn command queue failed: spawned actor does not implement ICommands.");
		return false;
	}

	for (int32 OrderIndex = 0; OrderIndex < M_CommandQueue.Num(); ++OrderIndex)
	{
		const bool bSetUnitToIdle = OrderIndex == 0;
		const ECommandQueueError CommandError = ExecuteSingleOrder(
			M_CommandQueue[OrderIndex],
			CommandsInterface,
			bSetUnitToIdle);

		if (CommandError == ECommandQueueError::NoError)
		{
			continue;
		}

		CompleteAsFailed(
			"Mission spawn command queue failed to execute order at index " + FString::FromInt(OrderIndex)
			+ " with ability " + Global_GetAbilityIDAsString(M_CommandQueue[OrderIndex].AbilityID));
		return false;
	}

	return true;
}

bool FMissionSpawnCommandQueueState::ApplyQueuedBehaviours()
{
	if (M_BehavioursToApply.IsEmpty())
	{
		return true;
	}

	AActor* SpawnedActor = M_SpawnedActor.Get();
	if (not IsValid(SpawnedActor))
	{
		CompleteAsFailed("Mission spawn command queue failed to apply startup behaviours: spawned actor is invalid.");
		return false;
	}

	UBehaviourComp* BehaviourComponent = SpawnedActor->FindComponentByClass<UBehaviourComp>();
	if (not IsValid(BehaviourComponent))
	{
		CompleteAsFailed(
			"Mission spawn command queue failed to apply startup behaviours: spawned actor has no behaviour component.");
		return false;
	}

	for (const TSubclassOf<UBehaviour>& BehaviourClass : M_BehavioursToApply)
	{
		if (not IsValid(BehaviourClass))
		{
			continue;
		}

		BehaviourComponent->AddBehaviour(BehaviourClass);
	}

	return true;
}

ECommandQueueError FMissionSpawnCommandQueueState::ExecuteSingleOrder(
	const FMissionSpawnCommandQueueOrder& Order,
	ICommands* CommandsInterface,
	const bool bSetUnitToIdle) const
{
	if (CommandsInterface == nullptr)
	{
		return ECommandQueueError::CommandDataInvalid;
	}

	switch (Order.AbilityID)
	{
	case EAbilityID::IdNoAbility:
		return ECommandQueueError::NoError;
	case EAbilityID::IdAttack:
		return CommandsInterface->AttackActor(Order.TargetActor, bSetUnitToIdle);
	case EAbilityID::IdAttackGround:
		return CommandsInterface->AttackGround(Order.TargetLocation, bSetUnitToIdle);
	case EAbilityID::IdMove:
		return CommandsInterface->MoveToLocation(Order.TargetLocation, bSetUnitToIdle, Order.TargetRotation, false);
	case EAbilityID::IdPatrol:
		return CommandsInterface->PatrolToLocation(Order.TargetLocation, bSetUnitToIdle);
	case EAbilityID::IdReverseMove:
		return CommandsInterface->ReverseUnitToLocation(Order.TargetLocation, bSetUnitToIdle);
	case EAbilityID::IdRotateTowards:
		return CommandsInterface->RotateTowards(Order.TargetRotation, bSetUnitToIdle);
	case EAbilityID::IdCapture:
		return CommandsInterface->CaptureActor(Order.TargetActor, bSetUnitToIdle);
	case EAbilityID::IdScavenge:
		return CommandsInterface->ScavengeObject(Order.TargetActor, bSetUnitToIdle);
	case EAbilityID::IdRepair:
		return CommandsInterface->RepairActor(Order.TargetActor, bSetUnitToIdle);
	case EAbilityID::IdReturnToBase:
		return CommandsInterface->ReturnToBase(bSetUnitToIdle);
	case EAbilityID::IdRetreat:
		return CommandsInterface->RetreatToLocation(Order.TargetLocation, bSetUnitToIdle);
	case EAbilityID::IdReinforceSquad:
		return CommandsInterface->Reinforce(bSetUnitToIdle);
	case EAbilityID::IdApplyBehaviour:
		return CommandsInterface->ActivateBehaviourAbility(Order.BehaviourAbilityType, bSetUnitToIdle);
	case EAbilityID::IdActivateMode:
		return CommandsInterface->ActivateModeAbility(Order.ModeAbilityType, bSetUnitToIdle);
	case EAbilityID::IdDisableMode:
		return CommandsInterface->DisableModeAbility(Order.ModeAbilityType, bSetUnitToIdle);
	case EAbilityID::IdFieldConstruction:
		return CommandsInterface->FieldConstruction(
			Order.FieldConstructionType,
			bSetUnitToIdle,
			Order.TargetLocation,
			Order.TargetRotation,
			Order.TargetActor);
	case EAbilityID::IdThrowGrenade:
		return CommandsInterface->ThrowGrenade(Order.TargetLocation, bSetUnitToIdle, Order.GrenadeAbilityType);
	case EAbilityID::IdCancelThrowGrenade:
		return CommandsInterface->CancelThrowingGrenade(bSetUnitToIdle, Order.GrenadeAbilityType);
	case EAbilityID::IdAimAbility:
		return CommandsInterface->AimAbility(Order.TargetLocation, bSetUnitToIdle, Order.AimAbilityType);
	case EAbilityID::IdCancelAimAbility:
		return CommandsInterface->CancelAimAbility(bSetUnitToIdle, Order.AimAbilityType);
	case EAbilityID::IdAttachedWeapon:
		return CommandsInterface->FireAttachedWeaponAbility(
			Order.TargetLocation,
			bSetUnitToIdle,
			Order.AttachedWeaponAbilityType);
	case EAbilityID::IdSwapTurret:
		return CommandsInterface->SwapTurret(bSetUnitToIdle, Order.TurretSwapAbilityType);
	case EAbilityID::IdDigIn:
		return CommandsInterface->DigIn(bSetUnitToIdle);
	case EAbilityID::IdBreakCover:
		return CommandsInterface->BreakCover(bSetUnitToIdle);
	case EAbilityID::IdSwitchWeapon:
		return CommandsInterface->SwitchWeapons(bSetUnitToIdle);
	case EAbilityID::IdFireRockets:
		return CommandsInterface->FireRockets(bSetUnitToIdle);
	case EAbilityID::IdCancelRocketFire:
		return CommandsInterface->CancelFireRockets(bSetUnitToIdle);
	case EAbilityID::IdConvertToVehicle:
		return CommandsInterface->ConvertToVehicle(bSetUnitToIdle);
	case EAbilityID::IdReturnCargo:
		return CommandsInterface->ReturnCargo(bSetUnitToIdle);
	case EAbilityID::IdPickupItem:
		return CommandsInterface->PickupItem(Cast<AItemsMaster>(Order.TargetActor), bSetUnitToIdle);
	case EAbilityID::IdEnterCargo:
		return CommandsInterface->EnterCargo(Order.TargetActor, bSetUnitToIdle);
	case EAbilityID::IdExitCargo:
		return CommandsInterface->ExitCargo(bSetUnitToIdle);
	case EAbilityID::IdManAbandonedTeamWeapon:
		return CommandsInterface->ManAbandonedTeamWeapon(Order.TargetActor, bSetUnitToIdle);
	case EAbilityID::IdTowActor:
		return CommandsInterface->TowActor(Order.TargetActor, Order.TowedActorTargetType, bSetUnitToIdle);
	case EAbilityID::IdDetachTow:
		return CommandsInterface->DetachTow(bSetUnitToIdle);
	default:
		return ECommandQueueError::CommandDataInvalid;
	}
}

void FMissionSpawnCommandQueueState::CompleteAsFailed(const FString& FailureMessage)
{
	bM_HasFinished = true;
	bM_HasFailed = true;
	RTSFunctionLibrary::ReportError(FailureMessage);
}
