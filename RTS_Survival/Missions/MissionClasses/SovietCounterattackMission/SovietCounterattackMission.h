// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/GameUI/InfoTakeover/InfoTakeoverSubsystem.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionCinematicTakeOverSession.h"

#include "SovietCounterattackMission.generated.h"

class AAircraftMaster;
class ACameraActor;
class AEnemyAirDrop;
class ALevelSequenceActor;
class ASovietCargoTrain;
class ATankMaster;
class UMissionCinematicTakeOverSession;
class UMovieSceneSequencePlayer;
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

	// 0 = not started, 1 = Dig In takeover open, 2 = camera tour, 3 = done (the prepare countdown runs).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack|Presentation")
	int32 GetDigInIntroStage() const { return static_cast<int32>(M_IntroStage); }

	// Dig-in markers currently in the world (they stay for the prepare phase).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack|Presentation")
	TArray<AActor*> GetDigInMarkers() const;

	// Dig-in positions a dug-in player vehicle holds right now (prepare phase).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack|Presentation")
	int32 GetDigInPositionsHeld() const;

	// True once the all-positions-held reward was paid (when the waves started).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack|Presentation")
	bool GetAllPositionsRewardPaid() const { return bM_AllPositionsRewardPaid; }

	// Pad the arrival cinematic films (index into the landing pads), -1 before it is chosen.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack|Presentation")
	int32 GetArrivalPadIndex() const { return M_ArrivalPadIndex; }

	// 0 = not started, 1 = waiting for the first wave's aircraft, 2 = train shot, 3 = drop shot, 4 = over.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack|Presentation")
	int32 GetArrivalCinematicStage() const { return static_cast<int32>(M_ArrivalStage); }

	// How the arrival cinematic ended (EMissionCinematicTakeOverResult as int; 0 while it has not ended).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack|Presentation")
	int32 GetArrivalCinematicResult() const { return M_ArrivalResult; }

	// The camera of the arrival cinematic while it plays (tests: screenshot / location checks).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack|Presentation")
	AActor* GetArrivalCinematicCamera() const;

	// The arrival cinematic's takeover session while it plays (tests can skip it like the player would).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack|Presentation")
	UMissionCinematicTakeOverSession* GetArrivalCinematicSession() const { return M_ArrivalSession; }

	/**
	 * @brief Plays the base reinforcements line and flies BaseReinforcements' vehicles for the player's faction into
	 * the base, one aircraft each. Runs on its own once the first wave's arrival cinematic is over; only once.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Soviet Counterattack|Presentation")
	void DropBaseReinforcements();

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Soviet Counterattack|Presentation")
	bool GetBaseReinforcementsDropped() const { return bM_BaseReinforcementsDropped; }

	// The first wave's arrival cinematic is over: it finished, was skipped, or never played (bWasShown false).
	UFUNCTION(BlueprintImplementableEvent, Category = "Soviet Counterattack|Presentation")
	void BP_OnArrivalCinematicOver(const bool bWasShown, const bool bWasSkipped);

protected:
	virtual void OnMissionStartedNative() override;
	virtual void OnCameraTourFinishedNative(const int32 ID, const bool bWasShown) override;

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
	// The "Prepare to defend" countdown (and the timer widget); runs once the dig-in intro is over.
	void Start_PrepareCountdown();
	int32 M_PrepareSeconds = 0;
	bool bM_PrepareCountdownStarted = false;

	// ----- Dig-in intro (takeover -> markers -> tour -> countdown) -----
	enum class EIntroStage : uint8
	{
		NotStarted,
		Takeover,
		Tour,
		Done
	};

	EIntroStage M_IntroStage = EIntroStage::NotStarted;
	int32 M_DigInTakeoverRequestId = INDEX_NONE;
	// The takeover can finish inside the queue call, before its request id is known.
	bool bM_IntroTakeoverQueuing = false;
	bool bM_IntroTakeoverFinishedWhileQueuing = false;
	FDelegateHandle M_DigInTakeoverHandle;
	FTimerHandle M_IntroVoiceLineTimer;

	// One slot per dig-in position (MarkerLocations order); a slot is empty when its marker could not spawn.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_DigInMarkers;

	// Minimap icon per position; NAME_None while the position is held (or the icon failed).
	TArray<FName> M_DigInMarkerIconIds;

	// Ground location of each position and whether a dug-in player vehicle holds it now.
	TArray<FVector> M_DigInPositions;
	TArray<bool> M_PositionHeld;
	FTimerHandle M_PositionPollTimer;
	bool bM_FirstPositionLinePlayed = false;
	bool bM_AllPositionsRewardPaid = false;

	void Positions_Poll();
	void Positions_SetHeld(const int32 Index, const bool bHeld);
	void Positions_SetIcon(const int32 Index, const bool bShow);
	void Positions_SetMarkerText(const int32 Index, const FText& Text) const;
	// Shows or hides a marker's ghost wall and arrow (every primitive but its text widget).
	void Positions_SetMarkerGhostVisible(const int32 Index, const bool bVisible) const;
	/** @return The position a dug-in player vehicle holds (closest in PositionRadius), or INDEX_NONE. */
	int32 Positions_FindHeldIndex(const ATankMaster* Tank) const;
	void Positions_RefreshObjective();
	// Pays AllPositionsHeldReward (and plays its line) when every position is held as the waves start.
	void Positions_OnWavesStart();

	void Intro_Start();
	void Intro_OnTakeoverFinished(const int32 RequestId, const EInfoTakeoverRequestResult Result);
	void Intro_SpawnMarkers();
	void Intro_RemoveMarkers();
	// Lifts the fog over the dig-in positions for the tour (and a while after), so the ridges are seen.
	void Intro_RevealPositions();
	void Intro_StartTour();
	// Polls until the tour shows its first shot, then starts the voice line after its delay.
	void Intro_WaitForFirstTourShot();
	void Intro_Finish();
	void Intro_UnbindTakeover();
	FVector Intro_GetGroundLocation(const FVector& Location) const;
	void SetObjective(const FText& Title, const FText& Description);

	// ----- Waves -----
	void OnPreparationOver();
	void SpawnNextWave();
	void OnSurviveTimerOver();
	TArray<FEnemyPlacementUnit> RollWaveUnits(const FSovietCounterattackDifficulty& Difficulty, const int32 WaveNumber) const;
	// Splits the units as evenly as possible over the three spawns (leftovers go to random spawns); everything goes to
	// the train with Spawns.bAllUnitsByTrain.
	void SplitOverSpawns(TArray<FEnemyPlacementUnit> Units, TArray<FEnemyPlacementUnit>& OutCenter,
	                     TArray<FEnemyPlacementUnit>& OutAirDrop, TArray<FEnemyPlacementUnit>& OutTrain) const;

	// ----- Groups and route -----
	FVector GetHQLocation() const;
	// The spawn's route points (AttackRoute, or TrainAttackRoute for the station), then the HQ, shifted by the spawn's
	// offset and projected.
	TArray<FVector> BuildRoute(const ESovietCounterattackSpawn Spawn) const;
	const TArray<FVector>& GetAttackRoutePoints(const ESovietCounterattackSpawn Spawn) const;
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
	// A settings point with z 0 is traced to the ground below it (then the navmesh); other points are used as is.
	FVector Spawn_GetGroundLocation(const FVector& Point) const;
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

	// The first wave's train waits for the arrival cinematic (see FSovietCounterattackArrivalCinematicSettings).
	bool bM_HoldTrainForCinematic = false;

	void Train_Start(const TArray<FEnemyPlacementUnit>& Units, const int32 WaveNumber);
	void Train_StartNextIfIdle();
	bool GetCanUseTrain() const;
	void Train_OnArrived(ASovietCargoTrain* Train);
	void Train_OnDeparted(ASovietCargoTrain* Train);
	void Train_Unload(ASovietCargoTrain* Train);
	FPendingTrainRun* Train_FindRun(const ASovietCargoTrain* Train);

	// ----- First wave arrival cinematic -----
	enum class EArrivalStage : uint8
	{
		NotStarted,
		Waiting,
		TrainShot,
		DropShot,
		Over
	};

	EArrivalStage M_ArrivalStage = EArrivalStage::NotStarted;
	int32 M_ArrivalResult = 0;
	FTimerHandle M_ArrivalPollTimer;
	FTimerHandle M_ArrivalCameraTimer;
	float M_ArrivalWaitSeconds = 0.f;
	// Pad (index into M_AirSlots / LandingPadCenters) the drop shot films.
	int32 M_ArrivalPadIndex = INDEX_NONE;
	FVector M_ArrivalPadGround = FVector::ZeroVector;
	// Closing speed of the filmed aircraft (cm/s, smoothed) and its last 2D distance to the pad.
	float M_ArrivalClosingSpeed = 0.f;
	float M_ArrivalLastPadDistance = -1.f;
	bool bM_ArrivalTrainReleased = false;
	// The first wave has a train, held back so the cinematic opens on it.
	bool bM_ArrivalFilmsTrain = false;
	// The mission timer widget was collapsed for the cinematic.
	bool bM_ArrivalHidTimer = false;
	ESlateVisibility M_ArrivalTimerVisibility = ESlateVisibility::SelfHitTestInvisible;
	FVector M_ArrivalTrainDirection = FVector::ForwardVector;
	bool bM_ArrivalUnitDelivered = false;
	// Sequence seconds at which the drop shot starts / the shot should end.
	float M_ArrivalDropShotStart = 0.f;
	FVector M_ArrivalTrainCameraLocation = FVector::ZeroVector;
	FVector M_ArrivalLookAt = FVector::ZeroVector;
	bool bM_ArrivalHasLookAt = false;

	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> M_ArrivalAircraft;

	// The filmed pad's unit once it is on the ground; the drop shot ends looking at it.
	UPROPERTY()
	TWeakObjectPtr<AActor> M_ArrivalDeliveredUnit;

	// Game speed before the cinematic slowed it (Cinematic.TimeDilation); restored when it ends.
	float M_ArrivalPreviousTimeDilation = 1.f;
	bool bM_ArrivalChangedTimeDilation = false;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_ArrivalCamera = nullptr;

	UPROPERTY()
	TObjectPtr<ALevelSequenceActor> M_ArrivalSequenceActor = nullptr;

	UPROPERTY()
	TObjectPtr<UMissionCinematicTakeOverSession> M_ArrivalSession = nullptr;

	void Arrival_Arm(const TArray<FEnemyPlacementUnit>& AirUnits, const bool bHasTrain);
	void Arrival_Poll();
	AAircraftMaster* Arrival_FindAircraft() const;
	void Arrival_ReleaseTrain();
	bool Arrival_StartCinematic();
	// The runtime camera both shots film through (field of view from the settings).
	bool Arrival_SpawnCamera();
	/** @brief Fixes the train shot's camera ahead of the train's front flatcar, beside and above the rails. */
	void Arrival_SetupTrainShot(const ASovietCargoTrain& Train, const float TrainShotSeconds);
	/** @return The 2D direction of travel of the train path at the point closest to Location. */
	FVector Arrival_GetTrackDirectionAt(const FVector& Location) const;
	void Arrival_TickCamera();
	void Arrival_EnterDropShot();
	void Arrival_OnUnitDelivered(AActor* Unit);
	void Arrival_SetSequenceLength(const float Seconds) const;
	float Arrival_GetSequenceSeconds() const;
	UMovieSceneSequencePlayer* Arrival_GetPlayer() const;
	UFUNCTION()
	void Arrival_OnSessionCompleted(EMissionCinematicTakeOverResult Result);
	// Stops waiting / playing; bReleaseTrain lets a held train go.
	void Arrival_Clear(const bool bReleaseTrain);
	// Once per mission, from every way the arrival cinematic can end (not from the clean-up).
	void Arrival_OnOver();
	bool bM_ArrivalOverHandled = false;

	// ----- Base reinforcements (BaseReinforcements) -----
	bool bM_BaseReinforcementsDropped = false;
	FTimerHandle M_BaseReinforcementsTimer;
	FVector BaseReinforcements_GetLandingPoint() const;

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
