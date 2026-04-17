#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "MissionCargoSquadWithVehicleSpawnState.generated.h"

class AMissionManager;
class ARTSAsyncSpawner;
class ASquadController;
class ATankMaster;
class UCargoSquad;

/**
 * @brief Tracks one mission request that spawns a tank and cargo squad, waits for full readiness,
 * then boards the squad into cargo and optionally orders a move.
 */
USTRUCT()
struct FMissionCargoSquadWithVehicleSpawnState
{
	GENERATED_BODY()

	void Init(
		const int32 InRequestId,
		AMissionManager* InMissionManager,
		const ETankSubtype InTankSubtype,
		const ESquadSubtype InSquadSubtype,
		const FVector& InSpawnLocation,
		const FRotator& InSpawnRotation,
		const FVector& InOptionalMoveLocationAfterEnter);
	bool StartAsyncSpawn(ARTSAsyncSpawner* RTSAsyncSpawner);
	bool HandleTankSpawnedActor(AActor* SpawnedActor);
	bool HandleSquadSpawnedActor(AActor* SpawnedActor);
	bool HandleSquadDataReady();
	bool TryProgressState();
	bool GetHasCompleted() const;
	bool GetHasFailed() const;
	bool GetIsFinished() const;
	bool GetBelongsToRequest(const int32 RequestId) const;

private:
	static constexpr float CargoSquadSpawnOffsetX = -80.0f;

	int32 M_RequestId = INDEX_NONE;

	UPROPERTY()
	TWeakObjectPtr<AMissionManager> M_MissionManager;

	UPROPERTY()
	FTrainingOption M_TankTrainingOption;

	UPROPERTY()
	FTrainingOption M_SquadTrainingOption;

	UPROPERTY()
	FVector M_VehicleSpawnLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector M_SquadSpawnLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator M_SpawnRotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector M_OptionalMoveLocationAfterEnter = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankActor;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_SquadController;

	UPROPERTY()
	TWeakObjectPtr<UCargoSquad> M_SquadCargoComponent;

	UPROPERTY()
	bool bM_TankSpawned = false;

	UPROPERTY()
	bool bM_SquadSpawned = false;

	UPROPERTY()
	bool bM_SquadDataReady = false;

	UPROPERTY()
	bool bM_HasIssuedEnterCargoOrder = false;

	UPROPERTY()
	bool bM_HasIssuedMoveOrder = false;

	UPROPERTY()
	bool bM_HasCompleted = false;

	UPROPERTY()
	bool bM_HasFailed = false;

	bool GetIsValidMissionManager() const;
	bool GetIsValidTankActor() const;
	bool GetIsValidSquadController() const;
	bool GetIsValidSquadCargoComponent();
	bool GetHasSquadEnteredCargo();
	bool GetShouldIssueMoveOrder() const;
	void SetStateAsFailed(const FString& FailureMessage);
};
