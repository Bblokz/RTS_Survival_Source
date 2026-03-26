#include "MissionTowTeamWeaponSpawnState.h"

#include "MissionManager.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/RTSComponents/TowMechanic/TowedActor/TowedActor.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void FMissionTowTeamWeaponSpawnState::Init(const int32 InRequestId, AMissionManager* InMissionManager,
	const ETankSubtype InTankSubtype, const ESquadSubtype InSquadSubtype, const FVector& InTankSpawnLocation)
{
	M_RequestId = InRequestId;
	M_MissionManager = InMissionManager;
	M_TankTrainingOption = FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(InTankSubtype));
	M_SquadTrainingOption = FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(InSquadSubtype));
	M_TankSpawnLocation = InTankSpawnLocation;
	M_SquadSpawnLocation = InTankSpawnLocation + FVector(-TeamWeaponSpawnOffsetX, 0.0f, 0.0f);
	M_TankActor = nullptr;
	M_SquadController = nullptr;
	M_TeamWeaponActor = nullptr;
	bM_TankSpawned = false;
	bM_SquadSpawned = false;
	bM_SquadDataReady = false;
	bM_HasCompleted = false;
	bM_HasFailed = false;
}

bool FMissionTowTeamWeaponSpawnState::StartAsyncSpawn(ARTSAsyncSpawner* RTSAsyncSpawner)
{
	if (not IsValid(RTSAsyncSpawner))
	{
		SetStateAsFailed("Mission tow spawn request could not start: async spawner is invalid.");
		return false;
	}

	if (not GetIsValidMissionManager())
	{
		return false;
	}

	const TWeakObjectPtr<AMissionManager> WeakMissionManager = M_MissionManager;
	const bool bTankSpawnRequestStarted = RTSAsyncSpawner->AsyncSpawnOptionAtLocation(
		M_TankTrainingOption,
		M_TankSpawnLocation,
		M_MissionManager.Get(),
		M_RequestId,
		[WeakMissionManager, RequestId = M_RequestId](const FTrainingOption&, AActor* SpawnedActor, const int32)->void
		{
			if (not WeakMissionManager.IsValid())
			{
				return;
			}
			WeakMissionManager->HandleSpawnTowedTeamWeaponTankSpawned(RequestId, SpawnedActor);
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
			WeakMissionManager->HandleSpawnTowedTeamWeaponSquadSpawned(RequestId, SpawnedActor);
		});

	return bTankSpawnRequestStarted && bSquadSpawnRequestStarted;
}

bool FMissionTowTeamWeaponSpawnState::HandleTankSpawnedActor(AActor* SpawnedActor)
{
	if (not IsValid(SpawnedActor))
	{
		SetStateAsFailed("Mission tow spawn request failed: spawned tank actor is invalid.");
		return false;
	}

	ATankMaster* TankActor = Cast<ATankMaster>(SpawnedActor);
	if (not IsValid(TankActor))
	{
		SetStateAsFailed("Mission tow spawn request failed: spawned actor is not ATankMaster.");
		return false;
	}

	M_TankActor = TankActor;
	bM_TankSpawned = true;
	return true;
}

bool FMissionTowTeamWeaponSpawnState::HandleSquadSpawnedActor(AActor* SpawnedActor)
{
	if (not IsValid(SpawnedActor))
	{
		SetStateAsFailed("Mission tow spawn request failed: spawned squad actor is invalid.");
		return false;
	}

	ASquadController* SquadController = Cast<ASquadController>(SpawnedActor);
	if (not IsValid(SquadController))
	{
		SetStateAsFailed("Mission tow spawn request failed: spawned actor is not ASquadController.");
		return false;
	}

	M_SquadController = SquadController;
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
			WeakMissionManager->HandleSpawnTowedTeamWeaponSquadReady(RequestId);
		},
		M_MissionManager.Get());
	return true;
}

bool FMissionTowTeamWeaponSpawnState::HandleSquadDataReady()
{
	if (not GetIsValidSquadController())
	{
		return false;
	}

	bM_SquadDataReady = true;
	return true;
}

bool FMissionTowTeamWeaponSpawnState::TryFinishInstantTow()
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

	if (not GetIsValidTankActor())
	{
		return false;
	}

	if (not GetIsValidSquadController())
	{
		return false;
	}

	if (not TryCacheTeamWeaponActorFromSquad())
	{
		return false;
	}

	ATeamWeaponController* TeamWeaponController = Cast<ATeamWeaponController>(M_SquadController.Get());
	if (not IsValid(TeamWeaponController))
	{
		SetStateAsFailed("Mission tow spawn request failed: squad is not a team weapon controller.");
		return false;
	}

	if (not M_TankActor->TryTowTeamWeaponInstant(TeamWeaponController, M_TeamWeaponActor.Get()))
	{
		return false;
	}

	bM_HasCompleted = true;
	return true;
}

bool FMissionTowTeamWeaponSpawnState::GetHasCompleted() const
{
	return bM_HasCompleted;
}

bool FMissionTowTeamWeaponSpawnState::GetHasFailed() const
{
	return bM_HasFailed;
}

bool FMissionTowTeamWeaponSpawnState::GetIsFinished() const
{
	return bM_HasCompleted || bM_HasFailed;
}

bool FMissionTowTeamWeaponSpawnState::GetBelongsToRequest(const int32 RequestId) const
{
	return M_RequestId == RequestId;
}

bool FMissionTowTeamWeaponSpawnState::GetIsValidMissionManager() const
{
	if (M_MissionManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Mission tow spawn state has no valid mission manager.");
	return false;
}

bool FMissionTowTeamWeaponSpawnState::GetIsValidTankActor() const
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
			"FMissionTowTeamWeaponSpawnState::GetIsValidTankActor",
			M_MissionManager.Get());
	}
	return false;
}

bool FMissionTowTeamWeaponSpawnState::GetIsValidSquadController() const
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
			"FMissionTowTeamWeaponSpawnState::GetIsValidSquadController",
			M_MissionManager.Get());
	}
	return false;
}

void FMissionTowTeamWeaponSpawnState::SetStateAsFailed(const FString& FailureMessage)
{
	bM_HasFailed = true;
	RTSFunctionLibrary::ReportError(FailureMessage);
}

bool FMissionTowTeamWeaponSpawnState::TryCacheTeamWeaponActorFromSquad()
{
	if (M_TeamWeaponActor.IsValid())
	{
		return true;
	}

	ATeamWeaponController* TeamWeaponController = Cast<ATeamWeaponController>(M_SquadController.Get());
	if (not IsValid(TeamWeaponController))
	{
		SetStateAsFailed("Mission tow spawn request failed: squad controller is not a team weapon controller.");
		return false;
	}

	UTowedActorComponent* TeamWeaponTowedActorComponent = TeamWeaponController->GetControlledTeamWeaponTowedActorComponentNoReport();
	if (not IsValid(TeamWeaponTowedActorComponent))
	{
		return false;
	}

	ATeamWeapon* TeamWeaponActor = Cast<ATeamWeapon>(TeamWeaponTowedActorComponent->GetOwner());
	if (not IsValid(TeamWeaponActor))
	{
		return false;
	}

	M_TeamWeaponActor = TeamWeaponActor;
	return true;
}
