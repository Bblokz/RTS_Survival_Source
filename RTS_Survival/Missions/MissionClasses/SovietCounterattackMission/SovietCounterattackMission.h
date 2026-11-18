// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"

#include "SovietCounterattackMission.generated.h"

class AEnemyAirDrop;
class ASovietCargoTrain;
class USovietCounterattackSettings;
class UW_MissionTimer;
struct FSovietCounterattackDifficulty;

/** The three places a wave's units come from; each place's share of a wave attacks as its own group. */
UENUM()
enum class ESovietCounterattackSpawn : uint8
{
	Center,
	AirDrop,
	Train
};

/** One unit of the landing field queue; the queue runs across waves, four units (one per pad) per batch. */
USTRUCT()
struct FSovietCounterattackQueuedAirUnit
{
	GENERATED_BODY()

	FEnemyPlacementUnit Unit;
	int32 WaveNumber = 0;
};

/** One pad of the running landing field batch. */
USTRUCT()
struct FSovietCounterattackAirSlot
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AEnemyAirDrop> AirDrop;

	UPROPERTY()
	TWeakObjectPtr<AActor> DeliveredUnit;

	// The unit is on the ground (or will never come: its air drop finished without it).
	bool bResolved = false;
	// Its aircraft delivered and left, or was shot down.
	bool bFinished = false;
};

/** A ground group (centre or train) whose units are still spawning. */
USTRUCT()
struct FSovietCounterattackGroundGroup
{
	GENERATED_BODY()

	int32 GroupId = INDEX_NONE;
	ESovietCounterattackSpawn Spawn = ESovietCounterattackSpawn::Center;
	int32 WaveNumber = 0;

	// One key per unit still spawning; each spawn resolves its key once (spawned or failed).
	TSet<int32> PendingSpawnKeys;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Units;
};

/** One Soviet unit marked on the minimap after the Survive timer ran out. */
USTRUCT()
struct FSovietCounterattackRemainingUnit
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Unit;

	FName IconId = NAME_None;
};

/**
 * @brief Defend-the-base mission: a "Prepare to defend" countdown, then WaveCount Soviet waves (one every
 * SecondsBetweenWaves) under a "Survive" countdown of WaveCount x SecondsBetweenWaves; afterwards the player kills the
 * Soviet units still alive (counted in the objective, marked on the minimap) to complete it.
 * Each wave's units are drawn from the difficulty's pool and split as evenly as possible over three spawns: a grid
 * at the centre spawn, the landing field (Pe-8s, one unit per pad, batches of four) and a cargo train to the station.
 * Each spawn's share attack-moves as its own group along the shared route (with that spawn's offset) to the player HQ.
 * @note All values are in USovietCounterattackSettings (Settings); the Blueprint child only sets it.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API USovietCounterattackMission : public UMissionBase
{
	GENERATED_BODY()

public:
	virtual void OnMissionComplete() override;
	virtual void OnMissionFailed() override;
	virtual void OnCleanUpMission() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Counterattack")
	TObjectPtr<USovietCounterattackSettings> Settings = nullptr;

	// Debug/tests: the first wave comes now instead of when the prepare countdown runs out.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Soviet Counterattack")
	void SkipPreparation();

	// 0 = preparing, 1 = waves (survive timer running), 2 = kill the remaining Soviets, 3 = done.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack")
	int32 GetPhase() const { return static_cast<int32>(M_Phase); }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack")
	int32 GetSpawnedWaveCount() const { return M_SpawnedWaveCount; }

	// Every Soviet unit this mission spawned that still lives.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack")
	TArray<AActor*> GetLivingSovietUnits() const;

	// Where each unit of GetLivingSovietUnits is, same order (a squad's soldiers' centre, not its controller).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack")
	TArray<FVector> GetLivingSovietUnitLocations() const;

	// Units still on their way (spawning, in an aircraft, on the train, or queued for the landing field).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack")
	int32 GetUndeliveredUnitCount() const;

protected:
	virtual void OnMissionStartedNative() override;

private:
	enum class EPhase : uint8
	{
		Preparing,
		Waves,
		KillRemaining,
		Done
	};

	EPhase M_Phase = EPhase::Preparing;

	// Difficulty values copied at the start.
	int32 M_WaveCount = 0;
	int32 M_SecondsBetweenWaves = 1;
	int32 M_SpawnedWaveCount = 0;

	FTimerHandle M_PrepareTimer;
	FTimerHandle M_WaveTimer;
	FTimerHandle M_SurviveEndTimer;
	FTimerHandle M_RemainingPollTimer;

	UPROPERTY()
	TWeakObjectPtr<UW_MissionTimer> M_TimerWidget;

	// Every unit this mission spawned (alive or not).
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_SpawnedUnits;

	int32 M_NextGroupId = 1;

	// ----- Start -----
	bool GetHasValidSettings() const;
	const FSovietCounterattackDifficulty* GetDifficulty() const;
	void Start_Preparation();
	void SetObjective(const FText& Title, const FText& Description);

	// ----- Waves -----
	void OnPreparationOver();
	void SpawnNextWave();
	void OnSurviveTimerOver();
	TArray<FEnemyPlacementUnit> RollWaveUnits(const FSovietCounterattackDifficulty& Difficulty, const int32 WaveNumber) const;
	// Splits the units as evenly as possible over the three spawns (leftovers go to random spawns).
	void SplitOverSpawns(TArray<FEnemyPlacementUnit> Units, TArray<FEnemyPlacementUnit>& OutCenter,
	                     TArray<FEnemyPlacementUnit>& OutAirDrop, TArray<FEnemyPlacementUnit>& OutTrain) const;

	// ----- Groups and route -----
	FVector GetHQLocation() const;
	TArray<FVector> BuildRoute(const FVector2D& Offset) const;
	FVector2D GetRouteOffset(const ESovietCounterattackSpawn Spawn) const;
	void RegisterSpawnedUnit(AActor* Unit);
	// Attack-moves the group along its spawn's route after OrderDelaySeconds.
	void OrderGroupAfterDelay(const TArray<TWeakObjectPtr<AActor>>& Units, const ESovietCounterattackSpawn Spawn,
	                          const int32 WaveNumber);
	void OrderGroup(const TArray<TWeakObjectPtr<AActor>>& Units, const ESovietCounterattackSpawn Spawn,
	                const int32 WaveNumber);
	FName MakeGroupMarkerId(const ESovietCounterattackSpawn Spawn, const int32 WaveNumber, const int32 GroupId) const;

	// Group markers currently on the minimap (removed when the remaining phase starts).
	TArray<FName> M_GroupMarkerIds;

	// ----- Ground spawns (centre grid, station grid) -----
	UPROPERTY()
	TArray<FSovietCounterattackGroundGroup> M_GroundGroups;

	void SpawnGroundGroup(const TArray<FEnemyPlacementUnit>& Units, const ESovietCounterattackSpawn Spawn,
	                      const int32 WaveNumber, const FVector& Center);
	TArray<FVector> BuildSpawnGrid(const FVector& Center, const int32 Count, const FVector& FacingTarget) const;
	void OnGroundUnitSpawned(const int32 GroupId, const int32 SpawnKey, AActor* SpawnedActor);
	void FinishGroundGroupIfDone(const int32 GroupId);

	// ----- Landing field -----
	TArray<FSovietCounterattackQueuedAirUnit> M_AirQueue;

	UPROPERTY()
	TArray<FSovietCounterattackAirSlot> M_AirSlots;

	// Wave of the running batch's units (the batch's first unit's wave).
	int32 M_AirBatchWave = 0;
	bool bM_AirBatchOrdered = false;
	int32 M_AirDropSerial = 0;

	void AirDrop_LaunchNextBatchIfIdle();
	void AirDrop_OnUnitDelivered(AEnemyAirDrop* AirDrop, AActor* Unit);
	UFUNCTION()
	void AirDrop_OnFinished(AEnemyAirDrop* AirDrop);
	FSovietCounterattackAirSlot* AirDrop_FindSlot(const AEnemyAirDrop* AirDrop);
	void AirDrop_CheckBatch();
	FVector AirDrop_GetPadLocation(const int32 PadIndex) const;
	void AirDrop_ClearBatch(const bool bDestroyAirDrops);

	// ----- Train -----
	struct FPendingTrainRun
	{
		TWeakObjectPtr<ASovietCargoTrain> Train;
		TArray<FEnemyPlacementUnit> Units;
		int32 WaveNumber = 0;
		FTimerHandle UnloadTimer;
		FTimerHandle DepartTimer;
	};

	TArray<FPendingTrainRun> M_TrainRuns;

	// Trains wait here while another one is on the rails (one track).
	struct FQueuedTrain
	{
		TArray<FEnemyPlacementUnit> Units;
		int32 WaveNumber = 0;
	};

	TArray<FQueuedTrain> M_TrainQueue;

	void Train_Start(const TArray<FEnemyPlacementUnit>& Units, const int32 WaveNumber);
	void Train_StartNextIfIdle();
	bool GetCanUseTrain() const;
	void Train_OnArrived(ASovietCargoTrain* Train);
	void Train_OnDeparted(ASovietCargoTrain* Train);
	void Train_Unload(ASovietCargoTrain* Train);
	FPendingTrainRun* Train_FindRun(const ASovietCargoTrain* Train);

	// ----- Kill the remaining Soviets -----
	UPROPERTY()
	TArray<FSovietCounterattackRemainingUnit> M_RemainingUnits;

	int32 M_NextRemainingIconIndex = 0;

	void Remaining_Start();
	void Remaining_Poll();
	void Remaining_AddUnit(AActor* Unit);
	void Remaining_RefreshObjective(const int32 AliveCount);

	void ClearRuntime(const bool bDestroyPendingActors);
};
