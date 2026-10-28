// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TankTrailBenchmarkSubsystem.generated.h"

class ACameraActor;
class ACPPController;
class ASceneCapture2D;
class UTextureRenderTarget2D;

UENUM()
enum class ETankTrailBenchPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForNavigation,
	WaitingForTanks,
	WarmingUp,
	Measuring,
	TakingPicture,
	Done
};

/** Frame costs of one measured phase, in milliseconds per frame. */
struct FTankTrailBenchPhase
{
	bool bTrailsOn = false;
	TArray<float> WallMs;
	TArray<float> GameThreadMs;
	TArray<float> RenderThreadMs;
	TArray<float> RhiThreadMs;
	TArray<float> GpuMs;
	double TrailUpdateSeconds = 0.0;
	int32 TrailUpdates = 0;
	int32 TrailSegments = 0;
	int32 LivingTanks = 0;
};

/** One spawned tank and the two ends of the lane it shuttles along. */
struct FTankTrailBenchTank
{
	TWeakObjectPtr<AActor> Tank;
	FVector LaneStart = FVector::ZeroVector;
	FVector LaneEnd = FVector::ZeroVector;
	bool bHeadingToEnd = true;
	int32 FramesSinceOrder = 0;
};

/**
 * @brief Measures what tank track trails cost: a column of tanks shuttles over the landscape while phases with the
 * trails off and on alternate (rts.TankTrails), so background load hits both alike. Logs wall, game thread,
 * render thread, RHI and GPU frame times per phase as RTS_TANK_TRAIL_BENCH lines, writes a CSV of every frame and
 * saves a picture of the trails plus the trail render target.
 * Started with -TankTrailBench; -TankTrailBenchDir=<folder>, -TankTrailBenchCount=<tanks>,
 * -TankTrailBenchFrames=<frames per phase>, -TankTrailBenchRepeats=<off/on pairs>, -TankTrailBenchAt=X,Y,
 * -TankTrailBenchProfileGPU (one ProfileGPU dump per phase), -TankTrailBenchCVar=<console variable to toggle instead of
 * rts.TankTrails, e.g. grass.Enable>.
 * Not in a shipping build.
 */
UCLASS()
class RTS_SURVIVAL_API UTankTrailBenchmarkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// The game boots paused behind the start widget; the benchmark has to tick to start it.
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	ETankTrailBenchPhase M_Phase = ETankTrailBenchPhase::Inactive;
	double M_PhaseStartRealSeconds = 0.0;
	double M_LastFrameSeconds = 0.0;
	int32 M_FramesInPhase = 0;
	bool bM_IsSuppressingErrorDialogs = false;
	bool bM_HasLoggedEngineUnpause = false;

	FString M_OutputDirectory;
	FVector M_DriveCentre = FVector::ZeroVector;
	int32 M_TankCount = 0;
	int32 M_FramesPerPhase = 0;
	int32 M_PhaseRepeats = 0;
	FString M_ToggledConsoleVariable;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TWeakObjectPtr<ACameraActor> M_Camera;

	UPROPERTY()
	TWeakObjectPtr<ASceneCapture2D> M_Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_CaptureTarget = nullptr;

	TArray<FTankTrailBenchTank> M_Tanks;
	TArray<FTankTrailBenchPhase> M_Phases;
	int32 M_PhaseIndex = INDEX_NONE;

	// Trail writer totals when the running phase started.
	double M_TrailSecondsAtPhaseStart = 0.0;
	int32 M_TrailUpdatesAtPhaseStart = 0;
	int32 M_TrailSegmentsAtPhaseStart = 0;

	void EnterPhase(ETankTrailBenchPhase NewPhase);
	double GetRealSecondsInPhase() const;
	void Finish(bool bSucceeded, const FString& Reason);

	void TickWaitingForGameStart();
	void TickWaitingForNavigation();
	void TickWaitingForTanks();
	void TickWarmingUp();
	void TickMeasuring();
	void TickTakingPicture();

	void SpawnTanks();
	// Mission units near the lanes would fight the tanks and pull them off their lanes.
	void ClearMapUnitsAroundLanes() const;
	void SpawnCamera();
	void HoldCamera() const;
	void KeepEngineUnpaused();
	void DriveTanks();
	void OrderTankAlongLane(FTankTrailBenchTank& BenchTank) const;
	int32 CountLivingTanks() const;

	// Sets the measured console variable (rts.TankTrails unless -TankTrailBenchCVar names another) to 1 or 0.
	void SetTrailsOn(bool bTrailsOn) const;
	void StartMeasuredPhase(int32 PhaseIndex);
	void RecordFrame(FTankTrailBenchPhase& Phase, double FrameMs) const;
	void FinishMeasuredPhase(FTankTrailBenchPhase& Phase) const;

	void ReportPhase(int32 PhaseIndex) const;
	void ReportSummary() const;
	// Median of (on - off) per adjacent pair of phases, for wall, game, render and GPU time.
	void ReportPairDeltas() const;
	void SaveFrameCsv() const;
	void SaveTrailPictures();
};
