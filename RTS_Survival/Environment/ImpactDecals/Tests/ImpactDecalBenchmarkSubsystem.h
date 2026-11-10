// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ImpactDecalBenchmarkSubsystem.generated.h"

class ACameraActor;
class ACPPController;
class ASceneCapture2D;
class UDecalComponent;
class UMaterialInterface;
class UTextureRenderTarget2D;
class UComboBoxString;
class UUserWidget;

UENUM()
enum class EImpactDecalBenchState : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForShaders,
	WarmingUp,
	RunningSteps,
	TakingPicture,
	// -ImpactDecalBenchLive: the map's own fighting leaves craters through the real weapon hooks.
	LiveCombat,
	// -ImpactDecalBenchLive: the escape menu's Shell Craters combo drives rts.ImpactDecals through Apply.
	MenuCheck,
	Done
};

// Pooled = ULandscapeImpactDecalSubsystem; Legacy = one UGameplayStatics::SpawnDecalAtLocation per crater and
// DestroyComponent of the oldest, the way UGameDecalManager spawns decals today.
enum class EImpactDecalBenchPath : uint8
{
	Pooled,
	Legacy
};

enum class EImpactDecalBenchStepKind : uint8
{
	// No craters; the reference frame cost.
	Baseline,
	// Count craters spread evenly over the step seconds into an empty pool.
	Fill,
	// Craters stay; steady state draw cost.
	Hold,
	// Count more craters over the step seconds with the cap at Count: every crater replaces the oldest.
	Recycle,
	// Count craters in one frame (after clearing), then a few frames to let the spike pass.
	Burst,
	// Remove every crater.
	Clear,
	// In one frame and timed as a whole: Count craters into an empty pool, Count more recycling them, then clearing,
	// each followed by the end-of-frame render state updates and a render thread flush. Immune to frame noise from
	// other programs; gives the full cost per crater (game thread + render thread work).
	Micro
};

struct FImpactDecalBenchStep
{
	EImpactDecalBenchStepKind Kind = EImpactDecalBenchStepKind::Baseline;
	EImpactDecalBenchPath Path = EImpactDecalBenchPath::Pooled;
	int32 Repeat = 0;
	FString Label;

	TArray<float> WallMs;
	TArray<float> GameThreadMs;
	TArray<float> RenderThreadMs;
	TArray<float> GpuMs;
	// Game thread time inside each crater request, in microseconds.
	TArray<float> SpawnMicros;
	int32 DecalsAtEnd = 0;
	int32 Merged = 0;
	// Frame index (inside the step) of the burst, for picking the spike out of the frame lists.
	int32 BurstFrame = INDEX_NONE;

	// Micro step: microseconds per crater for creating, recycling and clearing (flush overhead subtracted).
	float MicroCreateMicros = 0.f;
	float MicroRecycleMicros = 0.f;
	float MicroClearMicros = 0.f;
};

/** A landscape point the benchmark drops craters on, with its normal. */
struct FImpactDecalBenchPoint
{
	FVector Location = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	float CalibreMm = 20.f;
};

/**
 * @brief Measures what shell craters (ULandscapeImpactDecalSubsystem) cost: 100 craters over 10 seconds of game time,
 * then held, then 100 more recycling the oldest, then 100 in one frame; the same steps for the old way of spawning
 * decals (a new decal component per crater). Logs wall, game thread, render thread and GPU frame times and the game
 * thread time per crater as RTS_IMPACT_DECAL_BENCH lines, writes a CSV of every frame and a picture of the craters.
 * Started with -ImpactDecalBench; -ImpactDecalBenchDir=<folder>, -ImpactDecalBenchAt=X,Y, -ImpactDecalBenchCount=<n>,
 * -ImpactDecalBenchSeconds=<s>, -ImpactDecalBenchRepeats=<n>, -ImpactDecalBenchRadius=<cm spread around the centre>,
 * -ImpactDecalBenchProfileGPU (one ProfileGPU dump per Hold step), -ImpactDecalBenchPooledOnly,
 * -ImpactDecalBenchMicroOnly (only the noise-proof one-frame micro steps), -ImpactDecalBenchMicroRepeats=<n>,
 * -ImpactDecalBenchLive (no steps: let the map fight for -ImpactDecalBenchLiveSeconds=<s> game seconds and check every
 * crater is on a landscape, then drive the Shell Craters combo of the real settings menu: Off, 200, back to the
 * player's level; the runner backs up GameUserSettings.ini because Apply saves it),
 * -ImpactDecalBenchNoStart (do not press start game, e.g. on StartGame where backdrop battles fight on their own),
 * -ImpactDecalBenchSpawnFight (live mode on maps without fighting: German and Soviet tank waves every 30 s around the
 * centre; without -ImpactDecalBenchAt the centre falls back to the middle of the map's landscape).
 * Not in a shipping build.
 */
UCLASS()
class RTS_SURVIVAL_API UImpactDecalBenchmarkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	EImpactDecalBenchState M_State = EImpactDecalBenchState::Inactive;
	double M_StateStartRealSeconds = 0.0;
	double M_LastFrameSeconds = 0.0;
	int32 M_FramesInState = 0;
	bool bM_IsSuppressingErrorDialogs = false;

	FString M_OutputDirectory;
	FVector M_Centre = FVector::ZeroVector;
	int32 M_DecalCount = 100;
	float M_SpawnSeconds = 10.f;
	float M_SpreadRadius = 2500.f;
	int32 M_Repeats = 2;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TWeakObjectPtr<ACameraActor> M_Camera;

	UPROPERTY()
	TWeakObjectPtr<ASceneCapture2D> M_Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_CaptureTarget = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> M_DecalMaterial = nullptr;

	// Craters of the legacy path, oldest first.
	UPROPERTY()
	TArray<TObjectPtr<UDecalComponent>> M_LegacyDecals;

	TArray<FImpactDecalBenchPoint> M_Points;
	int32 M_NextPoint = 0;

	TArray<FImpactDecalBenchStep> M_Steps;
	int32 M_StepIndex = INDEX_NONE;
	int32 M_FramesInStep = 0;
	double M_StepStartGameSeconds = 0.0;
	int32 M_SpawnedInStep = 0;
	int32 M_MergedAtStepStart = 0;

	void EnterState(EImpactDecalBenchState NewState);
	double GetRealSecondsInState() const;
	void Finish(bool bSucceeded, const FString& Reason);

	void TickWaitingForGameStart();
	void TickWaitingForShaders();
	void TickWarmingUp();
	void TickRunningSteps();
	void TickTakingPicture();

	void BuildSteps();
	void AddStep(EImpactDecalBenchStepKind Kind, EImpactDecalBenchPath Path, int32 Repeat);
	bool CollectLandscapePoints();
	void SpawnCamera();
	void HoldCamera() const;
	void KeepEngineUnpaused() const;

	void StartStep(int32 StepIndex);
	// Spawns this frame's craters of a Fill / Recycle / Burst step.
	void SpawnForStep(FImpactDecalBenchStep& Step);
	void SpawnOne(FImpactDecalBenchStep& Step);
	void SpawnLegacy(const FImpactDecalBenchPoint& Point);
	void ClearAll();
	void RunMicroStep(FImpactDecalBenchStep& Step);
	// Runs the end-of-frame render state updates and waits for the render thread; returns the seconds it took.
	double FlushRenderWork() const;
	int32 GetStepFrames(const FImpactDecalBenchStep& Step) const;
	void RecordFrame(FImpactDecalBenchStep& Step, double FrameMs) const;
	int32 GetCurrentDecalCount(EImpactDecalBenchPath Path) const;
	static void SetPoolMax(int32 MaxDecals);

	// -ImpactDecalBenchLive.
	bool bM_IsLiveMode = false;
	// -ImpactDecalBenchNoStart: maps without a start-game button (StartGame's backdrop battles) run as they are.
	bool bM_SkipGameStart = false;
	bool bM_SpawnFight = false;
	bool bM_HasExplicitCentre = false;
	double M_LastFightWaveGameSeconds = 0.0;
	int32 M_FightWaves = 0;
	int32 M_FightUnitsSpawned = 0;

	// Moves the centre to the middle of the first landscape when nothing was given on the command line.
	bool CentreOnLandscape();
	void SpawnFightWave();
	float M_LiveSeconds = 90.f;
	double M_LiveStartGameSeconds = 0.0;
	TArray<FString> M_LiveFailures;

	UPROPERTY()
	TObjectPtr<UUserWidget> M_SettingsWidget = nullptr;

	void StartLiveCombat();
	void TickLiveCombat();
	void ReportLiveCraters();
	void TickMenuCheck();
	void RunMenuCheck();
	// Direct oldest-first test: cap 10, eleven craters at known spots, then cap 5; checks exactly which craters remain.
	void RunEvictionCheck();
	// Exact crater sizes (random spread off): larger shell craters from 75 mm, the bomb set by charge, merge order and
	// material swap on recycle between the shell and bomb sets.
	void RunSizeCheck();
	// Picks a combo option the way a click does, presses Apply (without the widget) and checks the console variable.
	bool SelectAndApplyImpactDecalsOption(UComboBoxString& Combo, const FString& Option, int32 ExpectedMax);
	void ExpectLive(bool bCondition, const FString& What);

	void ReportStep(const FImpactDecalBenchStep& Step) const;
	void ReportSummary() const;
	void SaveFrameCsv() const;
};
