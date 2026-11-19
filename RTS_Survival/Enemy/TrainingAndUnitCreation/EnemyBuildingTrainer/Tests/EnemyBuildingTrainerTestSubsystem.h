// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyBuildingTrainer/EnemyBuildingTrainerComponent.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "EnemyBuildingTrainerTestSubsystem.generated.h"

class ACPPController;
class AEnemyController;
class ASceneCapture2D;
class UEnemyStrategicAIComponent;
class UTextureRenderTarget2D;

enum class EEnemyBuildingTrainerTestStep : uint8
{
	StartGame,
	Spawn,
	WaitForBuildings,
	Train,
	Settle,
	RealThink,
	Cancel,
	SpawnerAudit,
	Done
};

/** One unit the AI can train, spawned through the async spawner to check it comes out as itself. */
struct FEnemyBuildingTrainerAuditEntry
{
	FTrainingOption Option;
	bool bHasSpawnerRow = false;
	bool bIsSpawnRequested = false;
	bool bHasCallback = false;
	TWeakObjectPtr<AActor> Unit;
};

/** Where a body of a trained unit was at the last spawn QA sample, and how fast it moved. */
struct FEnemyBuildingTrainerQABodySample
{
	TWeakObjectPtr<AActor> Body;
	FVector Location = FVector::ZeroVector;
	float Speed = 0.f;
};

/** One unit a test building trained (or dropped), as the trainer's events reported it. */
struct FEnemyBuildingTrainerTestJob
{
	FTrainingOption Option;
	float StartedAt = -1.f;
	float DurationSeconds = 0.f;
	float FinishedAt = -1.f;
	bool bCancelled = false;
	TWeakObjectPtr<AActor> Unit;
	FVector SpawnLocation = FVector::ZeroVector;
	FVector ExitLocation = FVector::ZeroVector;
	FVector ExitForward = FVector::ForwardVector;
	float RegisteredAt = -1.f;
	bool bMoveOrdered = false;
	float MaxMovedCm = 0.f;
	// The building's activity was on at every sample while this unit trained.
	bool bActivityOffWhileTraining = false;
	int32 CapturesTaken = 0;
	// Spawn QA (-EnemyBuildingTrainerTestSpawnQA): exit-view frames and measurements around the unit's appearance.
	bool bQAPreCaptured = false;
	int32 QAStagesDone = 0;
	FVector QASpawnCentroid = FVector::ZeroVector;
	// Sampled every tick for the first seconds: the largest move of any body beyond what its own speed explains (a
	// teleport, whatever the frame time) and how far above the ground any body got (a drop).
	float QAMaxBodyJumpCm = 0.f;
	float QAMaxAirborneCm = 0.f;
	float QALastSampleSeconds = -1.f;
	TArray<FEnemyBuildingTrainerQABodySample> QABodySamples;
};

/** One Soviet building the test spawns for player 2 and trains at. */
struct FEnemyBuildingTrainerTestCase
{
	FString Label;
	FString ClassPath;
	EEnemyBuildingTrainerKind ExpectedKind = EEnemyBuildingTrainerKind::Infantry;
	TWeakObjectPtr<AActor> Building;
	TWeakObjectPtr<UEnemyBuildingTrainerComponent> Trainer;
	bool bComponentAddedByTest = false;
	int32 TaggedActivityComponents = 0;
	TArray<FEnemyBuildingTrainerTestJob> Jobs;
	int32 ActivityOnCount = 0;
	int32 ActivityOffCount = 0;
	float LastActivityOffAt = -1.f;
	TArray<FString> Failures;
};

/**
 * @brief Headless check that the enemy AI trains at its buildings over time (-EnemyBuildingTrainerTest).
 *
 * Spawns the Soviet trainer buildings for player 2 on RTS_ToCopySmall, routes one unit per building through the
 * strategic AI (plus a second unit at the first building), and checks: the building shows activity at once, no unit
 * appears before its training time, the unit appears at the exit for player 2, joins player 2's direct-control pool and
 * can drive/walk away (tanks), and the building goes idle after its queue. Then one real training think (points,
 * pressure, routing) and a building destroyed mid-training (queue dropped, waiting units refunded, no spawn).
 *
 * Switches: -EnemyBuildingTrainerTestBuildings=Barracks,Factory (labels below or class paths; default all),
 * -EnemyBuildingTrainerTestAddMissing (adds a runtime trainer to buildings whose Blueprint has none),
 * -EnemyBuildingTrainerTestCapture (PNG frames idle / 30 % / 70 % / after / rest per building),
 * -EnemyBuildingTrainerTestExposure=<bias>, -EnemyBuildingTrainerTestAt=X,Y, -EnemyBuildingTrainerTestDir=<dir>,
 * -EnemyBuildingTrainerTestSpawnQA (two units per building, no test move orders; exit-view frames just before the
 * spawn and 0 / 1 / 3 / 8 / 20 s after, with ground offset, building overlap, stacking, fog, facing and movement
 * logged per frame as RTS_EBT spawnqa; fails on a teleport or drop after the spawn, a unit seen through the fog, units
 * overlapping at +20 s, or a unit still at or in front of its exit at +20 s).
 * Log lines start with RTS_EBT; the last one is RTS_EBT RESULT PASS|FAIL.
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyBuildingTrainerTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	UPROPERTY()
	TObjectPtr<ASceneCapture2D> M_CaptureActor;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_CaptureTarget;

#if RTS_WITH_MAP_TESTS
	EEnemyBuildingTrainerTestStep M_Step = EEnemyBuildingTrainerTestStep::StartGame;
	bool bM_IsActive = false;
	double M_StartRealSeconds = 0.0;
	float M_StepStartGameSeconds = 0.f;
	FVector M_GridOrigin = FVector::ZeroVector;
	FString M_OutputDirectory;
	bool bM_AddMissingTrainers = false;
	bool bM_Capture = false;
	float M_ExposureBias = 0.f;
	bool bM_HasExposureBias = false;
	TArray<FString> M_RequestedLabels;
	TArray<FEnemyBuildingTrainerTestCase> M_Cases;
	bool bM_SpawnerAudit = false;
	bool bM_SpawnQA = false;
	// Spawn QA fog control: a human vision bubble over this case's exit; its units must show, the others stay hidden.
	int32 M_FogControlCaseIndex = INDEX_NONE;
	TArray<FEnemyBuildingTrainerAuditEntry> M_AuditEntries;
	TArray<FString> M_GlobalFailures;
	TArray<FString> M_Lines;
	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<AEnemyController> M_EnemyController;
	TWeakObjectPtr<UEnemyStrategicAIComponent> M_StrategicAI;
	bool bM_SecondJobQueued = false;
	bool bM_RestCaptured = false;
	// Real think phase.
	int32 M_ThinkPointsBefore = 0;
	int32 M_ThinkJobsBefore = 0;
	int32 M_ThinkFinishedBefore = 0;
	// Cancel phase.
	int32 M_CancelCaseIndex = INDEX_NONE;
	int32 M_CancelRefundBefore = 0;
	int32 M_CancelExpectedRefund = 0;
	int32 M_CancelFinishedBefore = 0;
	int32 M_CancelCancelledBefore = 0;
	bool bM_CancelBuildingDestroyed = false;
	float M_CancelDestroyedAt = -1.f;

	void ReadCommandLine();
	void TickStartGame(const double RealSeconds);
	bool TryStartGame();
	void KeepGameRunning() const;
	void SpawnPhase();
	void CollectNavigableGridSlots(TArray<FVector>& OutSlots);
	bool SpawnCase(FEnemyBuildingTrainerTestCase& Case, const FVector& Location);
	void TickWaitForBuildings();
	void BindTrainer(const int32 CaseIndex);
	void QueueFirstJobs();
	void TickTrain();
	void TickJobChecks(FEnemyBuildingTrainerTestCase& Case);
	void TickSettle();
	void StartRealThink();
	void TickRealThink();
	void StartCancel();
	void TickCancel();
	void OnTrainerEvent(const int32 CaseIndex, const EEnemyBuildingTrainingEvent Event, const FTrainingOption& Option,
	                    AActor* Unit, const float Duration);
	void StartSpawnerAudit();
	void TickSpawnerAudit();
	void CaptureCase(const FEnemyBuildingTrainerTestCase& Case, const FString& Suffix);
	void TickSpawnQA(FEnemyBuildingTrainerTestCase& Case, const int32 JobIndex);
	void SpawnQAStage(FEnemyBuildingTrainerTestCase& Case, const int32 JobIndex, const TCHAR* StageTag);
	void CaptureExitView(const FEnemyBuildingTrainerTestCase& Case, const FString& Suffix);
	static void CollectUnitBodies(AActor* Unit, TArray<AActor*>& OutBodies);
	bool EnsureCaptureActor();
	bool GetAreAllJobsDone() const;
	int32 GetTotalJobCount() const;
	int32 GetTotalFinishedCount() const;
	int32 GetTotalCancelledCount() const;
	bool TryGetGroundZ(const FVector& Location, float& OutGroundZ) const;
	float GetGameSeconds() const;
	void SetStep(const EEnemyBuildingTrainerTestStep NewStep);
	void Log(const FString& Line);
	void Finish();
#endif
};
