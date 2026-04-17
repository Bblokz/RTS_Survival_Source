#include "MissionCargoSquadWithVehicleSpawnState.h"

#include "MissionManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoSquad/CargoSquad.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void FMissionCargoSquadWithVehicleSpawnState::Init(
	const int32 InRequestId,
	AMissionManager* InMissionManager,
	const ETankSubtype InTankSubtype,
	const ESquadSubtype InSquadSubtype,
	const FVector& InSpawnLocation,
	const FRotator& InSpawnRotation,
	const FVector& InOptionalMoveLocationAfterEnter)
{
	M_RequestId = InRequestId;
	M_MissionManager = InMissionManager;
	M_TankTrainingOption = FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(InTankSubtype));
	M_SquadTrainingOption = FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(InSquadSubtype));
	M_VehicleSpawnLocation = InSpawnLocation;
	M_SquadSpawnLocation = InSpawnLocation + InSpawnRotation.RotateVector(FVector(CargoSquadSpawnOffsetX, 0.0f, 0.0f));
	M_SpawnRotation = InSpawnRotation;
	M_OptionalMoveLocationAfterEnter = InOptionalMoveLocationAfterEnter;
	M_TankActor = nullptr;
	M_SquadController = nullptr;
	M_SquadCargoComponent = nullptr;
	bM_TankSpawned = false;
	bM_SquadSpawned = false;
	bM_SquadDataReady = false;
	bM_HasIssuedEnterCargoOrder = false;
	bM_HasIssuedMoveOrder = false;
	bM_HasCompleted = false;
	bM_HasFailed = false;
}

bool FMissionCargoSquadWithVehicleSpawnState::StartAsyncSpawn(ARTSAsyncSpawner* RTSAsyncSpawner)
{
	if (not IsValid(RTSAsyncSpawner))
	{
		SetStateAsFailed("Mission cargo spawn request could not start: async spawner is invalid.");
		return false;
	}

	if (not GetIsValidMissionManager())
	{
		return false;
	}

	const TWeakObjectPtr<AMissionManager> WeakMissionManager = M_MissionManager;
	const bool bTankSpawnRequestStarted = RTSAsyncSpawner->AsyncSpawnOptionAtLocation(
		M_TankTrainingOption,
		M_VehicleSpawnLocation,
		M_MissionManager.Get(),
		M_RequestId,
		[WeakMissionManager, RequestId = M_RequestId](const FTrainingOption&, AActor* SpawnedActor, const int32)->void
		{
			if (not WeakMissionManager.IsValid())
			{
				return;
			}

			WeakMissionManager->HandleSpawnCargoVehicleTankSpawned(RequestId, SpawnedActor);
		});

	const bool bSquadSpawnRequestStarted = RTSAsyncSpawner->AsyncSpawnOptionAtLocation(
		M_SquadTrainingOption,
		M_SquadSpawnLocation,
		M_MissionManager.Get(),
		M_RequestId,
		[WeakMissionManager, RequestId = M_RequestId](const FTrainingOption&, AActor* SpawnedActor, const int32)->void
		{
			if (not WeakMissionManager.IsValid())
			{
				return;
			}

			WeakMissionManager->HandleSpawnCargoVehicleSquadSpawned(RequestId, SpawnedActor);
		});

	return bTankSpawnRequestStarted && bSquadSpawnRequestStarted;
}

bool FMissionCargoSquadWithVehicleSpawnState::HandleTankSpawnedActor(AActor* SpawnedActor)
{
	if (not IsValid(SpawnedActor))
	{
		SetStateAsFailed("Mission cargo spawn request failed: spawned tank actor is invalid.");
		return false;
	}

	ATankMaster* TankActor = Cast<ATankMaster>(SpawnedActor);
	if (not IsValid(TankActor))
	{
		SetStateAsFailed("Mission cargo spawn request failed: spawned actor is not ATankMaster.");
		return false;
	}

	TankActor->SetActorRotation(M_SpawnRotation);
	M_TankActor = TankActor;
	bM_TankSpawned = true;
	return true;
}

bool FMissionCargoSquadWithVehicleSpawnState::HandleSquadSpawnedActor(AActor* SpawnedActor)
{
	if (not IsValid(SpawnedActor))
	{
		SetStateAsFailed("Mission cargo spawn request failed: spawned squad actor is invalid.");
		return false;
	}

	ASquadController* SquadController = Cast<ASquadController>(SpawnedActor);
	if (not IsValid(SquadController))
	{
		SetStateAsFailed("Mission cargo spawn request failed: spawned actor is not ASquadController.");
		return false;
	}

	SquadController->SetActorRotation(M_SpawnRotation);
	M_SquadController = SquadController;
	M_SquadCargoComponent = SquadController->FindComponentByClass<UCargoSquad>();
	bM_SquadSpawned = true;

	if (SquadController->GetIsSquadFullyLoaded())
	{
		bM_SquadDataReady = true;
		return true;
	}

	const TWeakObjectPtr<AMissionManager> WeakMissionManager = M_MissionManager;
	SquadController->SquadDataCallbacks.CallbackOnSquadDataLoaded(
		[WeakMissionManager, RequestId = M_RequestId]()->void
		{
			if (not WeakMissionManager.IsValid())
			{
				return;
			}

			WeakMissionManager->HandleSpawnCargoVehicleSquadReady(RequestId);
		},
		M_MissionManager.Get());

	return true;
}

bool FMissionCargoSquadWithVehicleSpawnState::HandleSquadDataReady()
{
	if (not GetIsValidSquadController())
	{
		return false;
	}

	bM_SquadDataReady = true;
	return true;
}

bool FMissionCargoSquadWithVehicleSpawnState::TryProgressState()
{
	if (bM_HasCompleted)
	{
		return true;
	}

	if (bM_HasFailed)
	{
		return false;
	}

	if (not bM_TankSpawned || not bM_SquadSpawned || not bM_SquadDataReady)
	{
		return false;
	}

	if (not GetIsValidTankActor() || not GetIsValidSquadController() || not GetIsValidSquadCargoComponent())
	{
		return false;
	}

	if (not bM_HasIssuedEnterCargoOrder)
	{
		const ECommandQueueError EnterCargoError = M_SquadController->EnterCargo(M_TankActor.Get(), true);
		if (EnterCargoError != ECommandQueueError::NoError)
		{
			SetStateAsFailed("Mission cargo spawn request failed: EnterCargo command returned error.");
			return false;
		}

		bM_HasIssuedEnterCargoOrder = true;
		return false;
	}

	if (not GetHasSquadEnteredCargo())
	{
		return false;
	}

	if (not GetShouldIssueMoveOrder())
	{
		bM_HasCompleted = true;
		return true;
	}

	if (bM_HasIssuedMoveOrder)
	{
		bM_HasCompleted = true;
		return true;
	}

	const ECommandQueueError MoveError = M_TankActor->MoveToLocation(
		M_OptionalMoveLocationAfterEnter,
		true,
		FRotator::ZeroRotator,
		false);
	if (MoveError != ECommandQueueError::NoError)
	{
		SetStateAsFailed("Mission cargo spawn request failed: MoveToLocation command returned error.");
		return false;
	}

	bM_HasIssuedMoveOrder = true;
	bM_HasCompleted = true;
	return true;
}

bool FMissionCargoSquadWithVehicleSpawnState::GetHasCompleted() const
{
	return bM_HasCompleted;
}

bool FMissionCargoSquadWithVehicleSpawnState::GetHasFailed() const
{
	return bM_HasFailed;
}

bool FMissionCargoSquadWithVehicleSpawnState::GetIsFinished() const
{
	return bM_HasCompleted || bM_HasFailed;
}

bool FMissionCargoSquadWithVehicleSpawnState::GetBelongsToRequest(const int32 RequestId) const
{
	return M_RequestId == RequestId;
}

bool FMissionCargoSquadWithVehicleSpawnState::GetIsValidMissionManager() const
{
	if (M_MissionManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Mission cargo spawn state has no valid mission manager.");
	return false;
}

bool FMissionCargoSquadWithVehicleSpawnState::GetIsValidTankActor() const
{
	if (M_TankActor.IsValid())
	{
		return true;
	}

	if (GetIsValidMissionManager())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			M_MissionManager.Get(),
			"M_TankActor",
			"FMissionCargoSquadWithVehicleSpawnState::GetIsValidTankActor",
			M_MissionManager.Get());
	}

	return false;
}

bool FMissionCargoSquadWithVehicleSpawnState::GetIsValidSquadController() const
{
	if (M_SquadController.IsValid())
	{
		return true;
	}

	if (GetIsValidMissionManager())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			M_MissionManager.Get(),
			"M_SquadController",
			"FMissionCargoSquadWithVehicleSpawnState::GetIsValidSquadController",
			M_MissionManager.Get());
	}

	return false;
}

bool FMissionCargoSquadWithVehicleSpawnState::GetIsValidSquadCargoComponent()
{
	if (M_SquadCargoComponent.IsValid())
	{
		return true;
	}

	if (not GetIsValidSquadController())
	{
		return false;
	}

	M_SquadCargoComponent = M_SquadController->FindComponentByClass<UCargoSquad>();
	if (M_SquadCargoComponent.IsValid())
	{
		return true;
	}

	if (GetIsValidMissionManager())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			M_MissionManager.Get(),
			"M_SquadCargoComponent",
			"FMissionCargoSquadWithVehicleSpawnState::GetIsValidSquadCargoComponent",
			M_MissionManager.Get());
	}

	return false;
}

bool FMissionCargoSquadWithVehicleSpawnState::GetHasSquadEnteredCargo()
{
	if (not GetIsValidSquadCargoComponent())
	{
		return false;
	}

	return M_SquadCargoComponent->GetIsInsideCargo();
}

bool FMissionCargoSquadWithVehicleSpawnState::GetShouldIssueMoveOrder() const
{
	return not M_OptionalMoveLocationAfterEnter.Equals(FVector::ZeroVector);
}

void FMissionCargoSquadWithVehicleSpawnState::SetStateAsFailed(const FString& FailureMessage)
{
	bM_HasFailed = true;
	RTSFunctionLibrary::ReportError(FailureMessage);
}
