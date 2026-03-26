#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "MissionTowTeamWeaponSpawnState.generated.h"

class AMissionManager;
class ARTSAsyncSpawner;
class ASquadController;
class ATankMaster;
class ATeamWeapon;

/**
 * @brief Tracks one mission request that spawns a tank and team weapon squad, then instantly links tow state.
 * Keeps all async callbacks and readiness checks in one place so mission manager and mission classes stay minimal.
 */
USTRUCT()
struct FMissionTowTeamWeaponSpawnState
{
	GENERATED_BODY()

	void Init(const int32 InRequestId, AMissionManager* InMissionManager, const ETankSubtype InTankSubtype,
		const ESquadSubtype InSquadSubtype, const FVector& InTankSpawnLocation);
	bool StartAsyncSpawn(ARTSAsyncSpawner* RTSAsyncSpawner);
	bool HandleTankSpawnedActor(AActor* SpawnedActor);
	bool HandleSquadSpawnedActor(AActor* SpawnedActor);
	bool HandleSquadDataReady();
	bool TryFinishInstantTow();
	bool GetHasCompleted() const;
	bool GetHasFailed() const;
	bool GetIsFinished() const;
	bool GetBelongsToRequest(const int32 RequestId) const;

private:
	static constexpr float TeamWeaponSpawnOffsetX = 50.0f;

	int32 M_RequestId = INDEX_NONE;

	UPROPERTY()
	TWeakObjectPtr<AMissionManager> M_MissionManager;

	UPROPERTY()
	FTrainingOption M_TankTrainingOption;

	UPROPERTY()
	FTrainingOption M_SquadTrainingOption;

	UPROPERTY()
	FVector M_TankSpawnLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector M_SquadSpawnLocation = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankActor;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_SquadController;

	UPROPERTY()
	TWeakObjectPtr<ATeamWeapon> M_TeamWeaponActor;

	UPROPERTY()
	bool bM_TankSpawned = false;

	UPROPERTY()
	bool bM_SquadSpawned = false;

	UPROPERTY()
	bool bM_SquadDataReady = false;

	UPROPERTY()
	bool bM_HasCompleted = false;

	UPROPERTY()
	bool bM_HasFailed = false;

	bool GetIsValidMissionManager() const;
	bool GetIsValidTankActor() const;
	bool GetIsValidSquadController() const;
	void SetStateAsFailed(const FString& FailureMessage);
	bool TryCacheTeamWeaponActorFromSquad();
};
