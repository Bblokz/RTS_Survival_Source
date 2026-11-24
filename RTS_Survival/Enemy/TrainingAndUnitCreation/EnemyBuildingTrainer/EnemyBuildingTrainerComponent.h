// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "EnemyBuildingTrainerComponent.generated.h"

class UEnemyStrategicAIComponent;
class USkeletalMeshComponent;

// What an AI building trains: squads or tanks.
UENUM(BlueprintType)
enum class EEnemyBuildingTrainerKind : uint8
{
	// Barracks and tents.
	Infantry,
	// Factories, assembly yards, platform factories.
	Vehicles
};

// What the tagged single-node skeletal meshes (cranes) do when the building stops training.
UENUM(BlueprintType)
enum class EEnemyTrainerIdleAnimation : uint8
{
	// Play on to the end of the current loop, then rest on the first frame; loops started together stay in step.
	FinishCycle,
	// Stop and jump back to the first frame.
	ReturnToStart,
	// Freeze on the current frame.
	PauseInPlace
};

UENUM()
enum class EEnemyBuildingTrainingEvent : uint8
{
	Queued,
	Started,
	Finished,
	Cancelled,
	ActivityOn,
	ActivityOff
};

// One unit the strategic AI paid for and handed to this building.
USTRUCT(BlueprintType)
struct FEnemyBuildingTrainingJob
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AI Training")
	FTrainingOption Option;

	UPROPERTY(BlueprintReadOnly, Category = "AI Training")
	float DurationSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AI Training")
	int32 TrainingPointCost = 0;

	int32 JobID = INDEX_NONE;
	// World seconds the job started training; negative while it waits in the queue.
	float StartWorldSeconds = -1.f;
	// The training time ran out and the async spawner is creating the unit.
	bool bIsSpawning = false;
	TWeakObjectPtr<UEnemyStrategicAIComponent> RequestingStrategicAI;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyBuildingTrainingStarted, const FTrainingOption&, Option,
                                             float, DurationSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyBuildingTrainingFinished, const FTrainingOption&, Option,
                                             AActor*, TrainedUnit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyBuildingTrainingCancelled, const FTrainingOption&, Option);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyBuildingTrainingActiveChanged, bool, bIsTraining);
// C++ observers (map tests): event, option, trained unit (Finished only), duration (Started only).
DECLARE_MULTICAST_DELEGATE_FourParams(FOnEnemyBuildingTrainingEventNative, EEnemyBuildingTrainingEvent,
                                      const FTrainingOption&, AActor*, float);

/**
 * @brief Lets a Soviet building train the units the enemy strategic AI picked, one at a time, over a real training
 * time, while the building shows that it is working (cranes, lights, smoke, sound).
 *
 * The strategic AI of the building's own player finds this component through the unit manager's buildings of that
 * player, pays the training points, and queues the unit here (EnqueueTraining). The building trains the queue in order;
 * when a unit's time is up it is spawned at the exit point and handed back to that strategic AI, which puts it in the
 * controller's direct-control pool.
 *
 * Activity without Blueprint code: components of the owner tagged
 * - AITrainingActivity: single-node skeletal meshes play their animation (all from frame 0, so several cranes stay in
 *   step), lights turn on, Niagara/Cascade effects activate, audio plays, any other scene component becomes visible.
 * - AITrainingHideWhileTraining: hidden while the building trains (a closed door, a tarp).
 * - AITrainingGlow: gets the scalar material parameter AITrainingActive 1 while training, 0 when idle.
 * Anything else (opening doors with a timeline, montages): bind OnTrainingActiveChanged / OnTrainingStarted in the
 * Blueprint, and call RefreshActivityState after the Blueprint shows or hides its machinery (construction, pack-up).
 *
 * @note The exit point is a scene component named AITrainingExit on the owner (its forward = the unit's facing);
 * without one, the unit leaves in front of the building's bounds.
 */
UCLASS(ClassGroup=(RTS), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnemyBuildingTrainerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyBuildingTrainerComponent();

	// Squads or tanks.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Training")
	EEnemyBuildingTrainerKind TrainerKind = EEnemyBuildingTrainerKind::Infantry;

	// Scales the training time of every unit at this building (1 = the cost-based time of EnemyAISettings).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Training", meta = (ClampMin = "0.05"))
	float TrainingTimeMlt = 1.f;

	// Name of the scene component the trained unit leaves from; its forward is the facing of the unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Training")
	FName ExitPointComponentName = TEXT("AITrainingExit");

	// Without an exit component: the unit appears this far in front of the building's bounds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Training")
	float FallbackExitDistanceCm = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Training|Activity")
	FName ActivityTag = TEXT("AITrainingActivity");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Training|Activity")
	FName HideWhileTrainingTag = TEXT("AITrainingHideWhileTraining");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Training|Activity")
	FName GlowTag = TEXT("AITrainingGlow");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Training|Activity")
	FName ActivityMaterialParameterName = TEXT("AITrainingActive");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Training|Activity")
	EEnemyTrainerIdleAnimation IdleAnimation = EEnemyTrainerIdleAnimation::FinishCycle;

	// Seconds into the tagged loops where the cranes rest while idle (FinishCycle / ReturnToStart). Training starts
	// from here too, so loops of the same length stay in step. Pick a moment with no part hanging in the air.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Training|Activity", meta = (ClampMin = "0"))
	float IdleRestSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Training|Activity", meta = (ClampMin = "0"))
	float AudioFadeOutSeconds = 1.f;

	// The building went from idle to training (true) or finished its queue / lost it (false).
	UPROPERTY(BlueprintAssignable, Category = "AI Training")
	FOnEnemyBuildingTrainingActiveChanged OnTrainingActiveChanged;

	// A unit started its training time.
	UPROPERTY(BlueprintAssignable, Category = "AI Training")
	FOnEnemyBuildingTrainingStarted OnTrainingStarted;

	// A unit left the building.
	UPROPERTY(BlueprintAssignable, Category = "AI Training")
	FOnEnemyBuildingTrainingFinished OnTrainingFinished;

	// A queued or training unit was dropped (building destroyed, packed up or removed).
	UPROPERTY(BlueprintAssignable, Category = "AI Training")
	FOnEnemyBuildingTrainingCancelled OnTrainingCancelled;

	FOnEnemyBuildingTrainingEventNative OnTrainingEventNative;

	UFUNCTION(BlueprintPure, Category = "AI Training")
	bool GetIsTraining() const { return not M_Queue.IsEmpty(); }

	// 0..1 of the unit in training; 0 when idle.
	UFUNCTION(BlueprintPure, Category = "AI Training")
	float GetActiveTrainingProgress() const;

	UFUNCTION(BlueprintPure, Category = "AI Training")
	FTrainingOption GetActiveTrainingOption() const;

	UFUNCTION(BlueprintPure, Category = "AI Training")
	int32 GetQueueLength() const { return M_Queue.Num(); }

	// Re-applies the idle/training look to the tagged components (after the Blueprint showed or hid them).
	UFUNCTION(BlueprintCallable, Category = "AI Training")
	void RefreshActivityState();

	// Standing, alive and the right kind for the option.
	bool GetCanTrainOption(const FTrainingOption& Option) const;
	bool GetCanTrainNow() const;
	// Seconds until the whole queue is through; used to pick the least busy building.
	float GetQueuedSecondsRemaining() const;
	// World time this building last took a unit; -1 when it never did. Breaks ties so equally free buildings take turns.
	float GetLastEnqueuedWorldSeconds() const { return M_LastEnqueuedWorldSeconds; }
	static float GetTrainingSecondsForCost(const int32 TrainingPointCost);

	/**
	 * @brief Queues a paid unit; the building starts on it at once when idle.
	 * @param Option Squad or tank to train.
	 * @param TrainingPointCost Points the AI paid; refunded when the job is dropped before it trains.
	 * @param RequestingStrategicAI Receives the trained unit and refunds.
	 * @return False when this building cannot train the option now (nothing was queued).
	 */
	bool EnqueueTraining(const FTrainingOption& Option, const int32 TrainingPointCost,
	                     UEnemyStrategicAIComponent* RequestingStrategicAI);

	// Drops the whole queue; points of units that had not started go back to the AI.
	void CancelAllTraining();

	FTransform GetExitTransform() const;

#if RTS_WITH_MAP_TESTS
	const TArray<FEnemyBuildingTrainingJob>& MapTest_GetQueue() const { return M_Queue; }
	bool MapTest_GetIsActivityOn() const { return bM_IsActivityOn; }
	int32 MapTest_GetTaggedActivityComponentCount() const;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TArray<FEnemyBuildingTrainingJob> M_Queue;

	int32 M_NextJobID = 1;
	float M_LastEnqueuedWorldSeconds = -1.f;
	bool bM_IsActivityOn = false;
	FTimerHandle M_ActiveJobTimer;
	FTimerHandle M_SpawnWatchdogTimer;
	FTimerHandle M_OwnerCheckTimer;
	// This building's exit in the cover finder: its player's soldiers take no cover where its units appear.
	int32 M_CoverKeepOutZoneId = 0;
	void UpdateCoverKeepOutZone();
	void RemoveCoverKeepOutZone();
	FTimerHandle M_FinishCycleTimer;

	void StartNextJob();
	void OnActiveJobTrainingTimeDone();
	void OnActiveJobSpawned(const int32 JobID, const FTrainingOption& Option, AActor* SpawnedActor,
	                        const FVector& ExitLocation);
	void OnSpawnWatchdog(const int32 JobID);
	void FinishActiveJob(AActor* TrainedUnit);
	void OnOwnerCheck();
	bool TryGetNavigableExitLocation(const FVector& DesiredLocation, FVector& OutLocation) const;
	int32 GetSpawnOwnerOverride() const;

	void SetActivity(const bool bActive);
	// bApplyGlow: material parameters are only written on changes and explicit refreshes (each write may make a MID).
	void ApplyActivityToComponents(const bool bActive, const bool bIsStateChange, const bool bApplyGlow);
	void ApplySkeletalActivity(USkeletalMeshComponent* SkeletalMesh, const bool bActive, const bool bIsStateChange,
	                           float& InOutLongestRemainingCycle) const;
	void OnFinishCycleTimer();
	// IdleRestSeconds clamped to this mesh's loop length.
	float GetRestPosition(USkeletalMeshComponent* SkeletalMesh) const;
	void StopTaggedSkeletalMeshes(const bool bReturnToStart) const;

	void Broadcast(const EEnemyBuildingTrainingEvent Event, const FTrainingOption& Option, AActor* Unit = nullptr,
	               const float Duration = 0.f);
	FString GetOwnerNameForLog() const;
};
