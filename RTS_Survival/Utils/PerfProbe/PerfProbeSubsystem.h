// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PerfProbeSubsystem.generated.h"

class ACameraActor;
class ACPPController;

UENUM()
enum class EPerfProbePhase : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForShaders,
	WarmingUp,
	Measuring,
	MeshShots,
	Timelapse,
	Done
};

/** One fixed camera the probe measures from. */
struct FPerfProbeView
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
};

/** One measured (view, variant) step: GPU / thread frame times in milliseconds. */
struct FPerfProbeStep
{
	int32 ViewIndex = 0;
	int32 VariantIndex = 0;
	int32 Repeat = 0;
	TArray<float> GpuMs;
	TArray<float> GameThreadMs;
	TArray<float> RenderThreadMs;
};

/**
 * @brief Render cost probe for the performance audit (Source/Docs/Performance/OptimizationLog.md): starts the game,
 * waits for shaders, takes the view from fixed cameras and measures GPU frame time per view. Optional console-command
 * variants are measured alternately on every view (A/B of a cvar such as r.MaterialQualityLevel), and one ProfileGPU
 * dump per step can be written to the log. Logs RTS_PERF_PROBE lines, then quits.
 * Switches: -PerfProbe, -PerfProbeViews="X,Y,Z,Pitch,Yaw;..." (default: the player camera at game start),
 * -PerfProbeFrames=<measured frames per step>, -PerfProbeWarmup=<frames>, -PerfProbeRepeats=<variant rounds>,
 * -PerfProbeVariants="cmd&cmd|cmd&cmd" (variants split by |, commands in a variant by &), -PerfProbeProfileGPU,
 * -PerfProbeKeepUI (default removes all widgets so only the scene is measured), -PerfProbeFov=<degrees>.
 * Mesh shots instead of measuring: -PerfProbeMeshes="/Game/..;/Game/.." -PerfProbeMeshAt=X,Y spawns each static mesh on
 * the ground there and saves a close-up and an RTS-distance screenshot (Saved/PerfAudit/MeshShots/<mesh>_<name>_<view>.png),
 * used to compare a mesh before and after a triangle reduction in real game lighting. Skeletal meshes are shot twice
 * per view, forced to LOD0 and to their last LOD (<mesh>_<name>_<view>_lod<N>.png).
 * -PerfProbeMeshSettle=<frames> (default 90) and -PerfProbeMeshCloseOnly speed up large batches.
 * Time-lapse instead: -PerfProbeTimelapse=<shots>,<seconds> saves the current view every <seconds> from map start
 * without starting a game or moving the camera (e.g. the main-menu backdrop battle; Saved/PerfAudit/Shots/<name>_tl<N>.png).
 * Not in a shipping build. Runner: Source/Tools/PerfAudit/run_gpu_probe.sh.
 */
UCLASS()
class RTS_SURVIVAL_API UPerfProbeSubsystem : public UTickableWorldSubsystem
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
	EPerfProbePhase M_Phase = EPerfProbePhase::Inactive;
	double M_PhaseStartRealSeconds = 0.0;
	int32 M_FramesInPhase = 0;
	bool bM_IsSuppressingErrorDialogs = false;
	bool bM_ProfilesGpu = false;
	bool bM_KeepsUI = false;
	int32 M_MeasureFrames = 300;
	int32 M_WarmupFrames = 240;
	int32 M_Repeats = 1;
	float M_FieldOfView = 90.f;

	TArray<FPerfProbeView> M_Views;
	TArray<TArray<FString>> M_Variants;
	TArray<FPerfProbeStep> M_Steps;
	int32 M_StepIndex = INDEX_NONE;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TWeakObjectPtr<ACameraActor> M_Camera;

	// Mesh-shot mode: meshes to photograph, the ground spot, and progress through meshes x views.
	TArray<FString> M_MeshPaths;
	FVector2D M_MeshSpot = FVector2D::ZeroVector;
	int32 M_MeshShotIndex = 0;
	// Skeletal meshes are shot at LOD0 and at their last LOD (forced), static meshes at their normal LOD.
	bool bM_MeshIsSkeletal = false;
	int32 M_MeshLodCount = 1;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_MeshActor;

	// Time-lapse mode: how many screenshots, the real-time interval between them, and how many were taken.
	int32 M_TimelapseShots = 0;
	double M_TimelapseIntervalSeconds = 5.0;
	int32 M_TimelapseShotsTaken = 0;

	void EnterPhase(EPerfProbePhase NewPhase);
	double GetRealSecondsInPhase() const;
	void Finish(bool bSucceeded, const FString& Reason);

	void ParseViews();
	void ParseVariants();
	void BuildSteps();
	void TickWaitingForGameStart();
	void TickWaitingForShaders();
	void TickWarmingUp();
	void TickMeasuring();
	void TickMeshShots();
	void TickTimelapse();
	bool ParseTimelapse();
	bool SpawnMeshForShot(int32 MeshIndex);
	void PlaceCameraForMeshShot(int32 ViewIndex, int32 LodVariant) const;
	void BeginStep(int32 StepIndex);
	void LogStep(const FPerfProbeStep& Step) const;
	void LogSummary() const;

	/** Undoes what mission intros do to the view every tick: pause, disabled world rendering, fades, view target. */
	void KeepViewRendering();
	void EnsureCamera();
};
