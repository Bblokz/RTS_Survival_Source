#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "MissionSpawnCommandQueueOrder.h"
#include "MissionSpawnCommandQueueState.generated.h"

class AMissionManager;
class ARTSAsyncSpawner;
class ASquadController;
class UBehaviour;
class UMissionBase;
class AActor;
class ICommands;
enum class ECommandQueueError : uint8;

/**
 * @brief Owns one async mission spawn request and executes queued startup commands once the actor is fully initialized.
 * The first order resets the command queue; all remaining orders append to preserve designer authored sequencing.
 */
USTRUCT()
struct FMissionSpawnCommandQueueState
{
	GENERATED_BODY()

	void Init(
		const int32 InRequestId,
		AMissionManager* InMissionManager,
		UMissionBase* InMissionOwner,
		const FTrainingOption& InTrainingOption,
		const int32 InSpawnId,
		const FVector& InSpawnLocation,
		const FRotator& InSpawnRotation,
		const TArray<TSubclassOf<UBehaviour>>& InBehavioursToApply,
		const TArray<FMissionSpawnCommandQueueOrder>& InCommandQueue);
	bool StartAsyncSpawn(ARTSAsyncSpawner* RTSAsyncSpawner);
	bool HandleSpawnedActor(AActor* SpawnedActor);
	bool TickExecution();
	bool GetBelongsToRequest(const int32 RequestId) const;
	bool GetIsFinished() const;

private:
	static constexpr int32 RequiredTicksBeforeQueueExecution = 1;

	int32 M_RequestId = INDEX_NONE;

	UPROPERTY()
	TWeakObjectPtr<AMissionManager> M_MissionManager;

	UPROPERTY()
	TWeakObjectPtr<UMissionBase> M_MissionOwner;

	UPROPERTY()
	FTrainingOption M_TrainingOption;

	UPROPERTY()
	int32 M_SpawnId = INDEX_NONE;

	UPROPERTY()
	FVector M_SpawnLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator M_SpawnRotation = FRotator::ZeroRotator;

	UPROPERTY()
	TArray<TSubclassOf<UBehaviour>> M_BehavioursToApply;

	UPROPERTY()
	TArray<FMissionSpawnCommandQueueOrder> M_CommandQueue;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_SpawnedActor;

	UPROPERTY()
	int32 M_TicksSinceSpawn = 0;

	UPROPERTY()
	bool bM_HasSpawnCallbackExecuted = false;

	UPROPERTY()
	bool bM_HasFinished = false;

	UPROPERTY()
	bool bM_HasFailed = false;

	bool GetIsValidMissionManager() const;
	bool GetIsValidMissionOwner() const;
	bool GetIsValidSpawnedActor() const;
	bool GetCanExecuteQueueThisTick();
	bool GetCanExecuteQueueOnSquadController(ASquadController* SquadController) const;
	void NotifyMissionSpawnCompleted(AActor* SpawnedActor);
	void SetSpawnedActorRotation();
	bool ApplyQueuedBehaviours();
	bool ExecuteQueuedOrders();
	ECommandQueueError ExecuteSingleOrder(const FMissionSpawnCommandQueueOrder& Order, ICommands* CommandsInterface,
	                                      const bool bSetUnitToIdle) const;
	void CompleteAsFailed(const FString& FailureMessage);
};
