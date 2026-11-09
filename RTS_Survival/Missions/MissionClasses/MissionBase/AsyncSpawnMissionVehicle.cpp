#include "AsyncSpawnMissionVehicle.h"

#include "MissionBase.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

UAsyncSpawnMissionVehicle* UAsyncSpawnMissionVehicle::SpawnVehicleVariant(
	UMissionBase* Mission,
	const TSoftClassPtr<ATankMaster> VehicleClass,
	const FVector SpawnLocation,
	const FRotator SpawnRotation)
{
	UAsyncSpawnMissionVehicle* Action = NewObject<UAsyncSpawnMissionVehicle>();
	Action->M_Mission = Mission;
	Action->M_VehicleClass = VehicleClass;
	Action->M_SpawnLocation = SpawnLocation;
	Action->M_SpawnRotation = SpawnRotation;
	// Keeps the action alive until SetReadyToDestroy; without a world it stays unregistered and fails in Activate.
	if (IsValid(Mission))
	{
		Action->RegisterWithGameInstance(Mission);
	}
	return Action;
}

void UAsyncSpawnMissionVehicle::Activate()
{
	if (not GetIsValidMission())
	{
		FinishAsFailed("the mission is invalid.");
		return;
	}

	if (M_VehicleClass.IsNull())
	{
		FinishAsFailed("no vehicle class was given.");
		return;
	}

	if (StartAsyncSpawn())
	{
		return;
	}

	// The spawner can call back with nullptr before returning false; that already failed the node.
	if (not bM_HasResult)
	{
		FinishAsFailed("the async spawner could not start the request.");
	}
}

bool UAsyncSpawnMissionVehicle::StartAsyncSpawn()
{
	ARTSAsyncSpawner* RTSAsyncSpawner = FRTS_Statics::GetAsyncSpawner(M_Mission.Get());
	if (not IsValid(RTSAsyncSpawner))
	{
		RTSFunctionLibrary::ReportError("Spawn Vehicle Variant found no async spawner for mission "
			+ M_Mission->GetName());
		return false;
	}

	const TWeakObjectPtr<UAsyncSpawnMissionVehicle> WeakThis(this);
	// The option only names the request in spawner errors; the class decides what spawns.
	const FTrainingOption UnnamedTrainingOption;
	constexpr int32 UnusedSpawnId = INDEX_NONE;
	return RTSAsyncSpawner->AsyncSpawnClassAtLocation(
		TSoftClassPtr<AActor>(M_VehicleClass.ToSoftObjectPath()),
		UnnamedTrainingOption,
		M_SpawnLocation,
		this,
		UnusedSpawnId,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)-> void
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->OnVehicleActorSpawned(SpawnedActor);
		}, M_SpawnRotation);
}

void UAsyncSpawnMissionVehicle::OnVehicleActorSpawned(AActor* SpawnedActor)
{
	if (not IsValid(SpawnedActor))
	{
		FinishAsFailed("the spawner returned no actor.");
		return;
	}

	ATankMaster* SpawnedTank = Cast<ATankMaster>(SpawnedActor);
	if (not IsValid(SpawnedTank))
	{
		FinishAsFailed("the spawned actor " + SpawnedActor->GetName() + " is not a tank.");
		return;
	}

	// Same as the other mission spawns: the rotation is set again once the unit exists.
	SpawnedTank->SetActorRotation(M_SpawnRotation, ETeleportType::ResetPhysics);
	FinishWithResult(SpawnedTank);
}

void UAsyncSpawnMissionVehicle::FinishWithResult(ATankMaster* SpawnedTank)
{
	if (bM_HasResult)
	{
		return;
	}
	bM_HasResult = true;
	M_SpawnedTank = SpawnedTank;

	// Waiting a tick lets the unit finish its spawn setup before the mission orders it, and keeps the pins
	// from firing before Then when the spawner answers synchronously.
	// No validator: a mission cleaned up mid-spawn is expected and must not report an error.
	UWorld* World = M_Mission.IsValid() ? M_Mission->GetWorld() : nullptr;
	if (not IsValid(World))
	{
		BroadcastResult();
		return;
	}
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UAsyncSpawnMissionVehicle::BroadcastResult));
}

void UAsyncSpawnMissionVehicle::FinishAsFailed(const FString& Reason)
{
	RTSFunctionLibrary::ReportError("Spawn Vehicle Variant failed for " + GetVehicleClassName() + ": " + Reason);
	FinishWithResult(nullptr);
}

void UAsyncSpawnMissionVehicle::BroadcastResult()
{
	// A mission that is gone has no event graph left to run; that is expected, so no validator error here.
	if (M_Mission.IsValid())
	{
		if (M_SpawnedTank.IsValid())
		{
			OnSpawned.Broadcast(M_SpawnedTank.Get());
		}
		else
		{
			OnFailed.Broadcast(nullptr);
		}
	}
	SetReadyToDestroy();
}

FString UAsyncSpawnMissionVehicle::GetVehicleClassName() const
{
	if (M_VehicleClass.IsNull())
	{
		return "<no class>";
	}
	return M_VehicleClass.ToSoftObjectPath().GetAssetName();
}

bool UAsyncSpawnMissionVehicle::GetIsValidMission() const
{
	if (M_Mission.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_Mission",
		"UAsyncSpawnMissionVehicle::GetIsValidMission",
		this);
	return false;
}
