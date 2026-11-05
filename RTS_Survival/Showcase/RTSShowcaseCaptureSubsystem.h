// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/SteamCapture/RTSSteamCaptureFrameWriter.h"
#include "RTS_Survival/SteamCapture/RTSSteamCaptureViewport.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSShowcaseCaptureSubsystem.generated.h"

class ACPPTurretsMaster;
class APawn;
class APlayerController;
class ARTSShowcaseCamera;

UENUM()
enum class ERTSShowcasePhase : uint8
{
	Idle,
	WaitingForGame,
	LoadingStage,
	WarmingUp,
	Settling,
	Rendering,
	Finishing
};

/** Options of one batch of showcase stills; console arguments and command-line switches fill it. */
USTRUCT()
struct FRTSShowcaseSessionSettings
{
	GENERATED_BODY()

	// Below 1 renders quick previews at a fraction of each camera's resolution.
	float M_ResolutionScale = 1.f;
	// Frames rendered at the output size before the saved one, so temporal AA and Lumen converge.
	int32 M_RenderFrames = 48;
	// Real seconds after switching camera and stage lighting (exposure, sky light, Lumen, streaming).
	double M_SettleSeconds = 2.5;
	// Real seconds of normal play before the first shot, so units finish spawning their parts and squads.
	double M_WarmupSeconds = 6.0;
	bool bM_FreezeActors = true;
	bool bM_DisableTextureStreaming = false;
	bool bM_QuitWhenDone = false;
	FString M_OutputDirectory;
};

/**
 * @brief Renders the ARTSShowcaseCamera presets of the showcase map to PNG stills at their own resolution.
 * Use the console in PIE (RTS.Showcase.Shot <all|stage|shot> [scale]) or -ShowcaseShots=<filter> on a -game run,
 * which quits when done. The world is frozen after a warm-up so units hold their pose between shots.
 */
UCLASS()
class RTS_SURVIVAL_API URTSShowcaseCaptureSubsystem : public UTickableWorldSubsystem
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

	/**
	 * @brief Queues every showcase camera whose stage, shot name or label matches the filter.
	 * @param Filter "all", a stage name, a shot id or a wildcard such as "Soviet*_V".
	 * @param Settings Batch options; the output directory defaults to Saved/Showcase/<timestamp>.
	 * @return Number of queued cameras; 0 when nothing matched or a batch is already running.
	 */
	int32 RequestShots(const FString& Filter, const FRTSShowcaseSessionSettings& Settings);

	void LogCameras() const;

private:
	ERTSShowcasePhase M_Phase = ERTSShowcasePhase::Idle;
	FRTSShowcaseSessionSettings M_Settings;

	UPROPERTY()
	TArray<TWeakObjectPtr<ARTSShowcaseCamera>> M_PendingCameras;

	UPROPERTY()
	TWeakObjectPtr<ARTSShowcaseCamera> M_ActiveCamera;

	UPROPERTY()
	TWeakObjectPtr<APlayerController> M_PlayerController;

	TUniquePtr<FRTSSteamCaptureViewport> M_CaptureViewport;
	TSharedPtr<FRTSSteamCaptureViewExtension, ESPMode::ThreadSafe> M_CaptureViewExtension;
	FRTSSteamCaptureFrameWriter M_FrameWriter;
	FDelegateHandle M_EndFrameDelegateHandle;
	FIntPoint M_OutputResolution = FIntPoint::ZeroValue;

	// Where every pawn stood at begin play; the still prep puts units back that moved during the warm-up.
	TMap<TWeakObjectPtr<APawn>, FTransform> M_PlacedPawnTransforms;

	// Turret aim along the hull at stage load, restored for the stills so guns point at the hero cameras.
	TMap<TWeakObjectPtr<ACPPTurretsMaster>, FRotator> M_PlacedTurretRotations;

	// Console variables changed for the batch, with the value to restore afterwards.
	TMap<FString, FString> M_SavedConsoleVariables;

	// Stage whose sublevel is streamed in (or whose units were prepared, on a map without stage sublevels).
	FName M_LoadedStage;
	double M_WarmupSecondsForStage = 0.0;
	double M_PhaseStartSeconds = 0.0;
	int32 M_RenderedFrameCount = 0;
	int32 M_WrittenShotCount = 0;
	bool bM_HasPreparedActors = false;
	bool bM_HasStartedGame = false;

	/** Spawns the units that stand in the level as ShowcaseSpawn= tagged markers. */
	void SpawnMarkedUnits(UWorld& World);
	void RecordPlacedPawns(UWorld& World);
	/** Puts moved pawns back and turns turrets back along their hulls. @return Number of pawns put back. */
	int32 RestorePlacedPawns() const;

	void SetPhase(ERTSShowcasePhase NewPhase);
	double GetSecondsInPhase() const;

	void TickWaitingForGame();
	void TickLoadingStage();
	void TickWarmingUp();

	/**
	 * @brief Streams in the sublevel RTS_Showcase_<StageName> and out every other stage sublevel.
	 * @return False when the map has no sublevel for this stage (everything lives in the persistent level).
	 */
	bool RequestStageLevel(FName StageName);
	bool GetIsRequestedStageLevelReady() const;
	void StartActiveShot();
	void TickSettling();
	void TickFinishing();

	bool TryStartGame();
	void PrepareActorsForStills();
	void ApplyCaptureConsoleVariables();
	void SetConsoleVariableForBatch(const TCHAR* Name, const TCHAR* Value);
	void RestoreConsoleVariables();

	bool BeginNextShot();
	void KeepViewOnActiveCamera() const;
	void AddStillViewToTextureStreaming() const;
	bool CreateCaptureViewport();
	void DestroyCaptureViewport();

	void OnEndFrame();
	bool DrawCaptureFrame(bool bReadPixels, TArray<FColor>& OutPixels);
	void FinishActiveShot(TArray<FColor>&& Pixels);

	bool GetIsValidPlayerController() const;
};
