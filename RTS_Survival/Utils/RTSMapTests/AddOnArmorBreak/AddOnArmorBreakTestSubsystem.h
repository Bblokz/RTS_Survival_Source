// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "AddOnArmorBreakTestTypes.h"

#include "AddOnArmorBreakTestSubsystem.generated.h"

class AAddOnArmorTestVehicle;
class ACPPController;
class ASceneCapture2D;
class ASkyLight;
class UTextureRenderTarget2D;

enum class EAddOnArmorBreakTestMode : uint8
{
	// One vehicle of one technique drives a circle and is hit; checks that pieces fall and intact ones follow.
	Spike,
	// One vehicle per technique side by side, the same hits on both, pictured at set moments.
	Capture,
	// Many vehicles of one technique driving and being hit; per-frame cost is written to a CSV.
	Perf
};

enum class EAddOnArmorBreakTestPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	Settling,
	WarmingUp,
	Idle,
	Hitting,
	SettlingDebris,
	Done
};

/** A camera of the capture: aimed at a point with yaw/pitch/distance, rendering every frame. */
struct FAddOnArmorCaptureCamera
{
	FString Name;
	FVector LookAt = FVector::ZeroVector;
	float Yaw = 0.f;
	float Pitch = -70.f;
	float Distance = 3000.f;
	float FieldOfView = 90.f;
	TWeakObjectPtr<ASceneCapture2D> Capture;
	TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;
};

/** A scripted capture hit, given relative to the vehicle so both techniques get the same shell. */
struct FAddOnArmorScriptedHit
{
	FString Name;
	float GameSeconds = 0.f;
	// Hit point and shell direction in vehicle space.
	FVector LocalLocation = FVector::ZeroVector;
	FVector LocalDirection = FVector::LeftVector;
	float BreakRadius = 20.f;
	float PushSpeed = 450.f;
	int32 MaxPieces = 6;
};

/** Frame costs of one phase of a perf run. */
struct FAddOnArmorPerfPhaseStats
{
	TArray<float> FrameMs;
	TArray<float> GameThreadMs;
	TArray<float> RenderThreadMs;
	TArray<float> RhiThreadMs;
	TArray<float> GpuMs;
	// Process CPU use in cores; includes the async physics thread, where the geometry collection's cost lands.
	TArray<float> CpuCores;
	int32 MaxLiveDebris = 0;
	int32 MaxParticles = 0;
};

/**
 * @brief Compares two ways of breaking add-on armour (side skirts) off a tank: pooled Niagara mesh debris and a
 * geometry collection. Started with -AddOnArmorBreakTest=Spike|Capture|Perf in an offscreen game run (runners in
 * Tools/AddOnArmorBreak); spawns AAddOnArmorTestVehicle stand-ins, fires synthetic hits and writes pictures or
 * per-frame costs to Saved/AddOnArmorBreak. Not created in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UAddOnArmorBreakTestSubsystem : public UTickableWorldSubsystem
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
	EAddOnArmorBreakTestMode M_Mode = EAddOnArmorBreakTestMode::Spike;
	EAddOnArmorBreakTestPhase M_Phase = EAddOnArmorBreakTestPhase::Inactive;
	EAddOnArmorTestTechnique M_Technique = EAddOnArmorTestTechnique::Niagara;
	EAddOnArmorTestGcCollision M_GcCollision = EAddOnArmorTestGcCollision::FromStart;
	double M_PhaseStartRealSeconds = 0.0;
	float M_PhaseStartGameSeconds = 0.f;
	double M_LastTickRealSeconds = 0.0;

	struct FRunSettings
	{
		int32 VehicleCount = 25;
		float HitsPerSecond = 10.f;
		bool bDrive = true;
		int32 Seed = 1;
		float IdleSeconds = 10.f;
		float HitSeconds = 20.f;
		float SettleSeconds = 8.f;
		FString Tag;
		FString OutputDirectory;
		// Capture: the techniques shown side by side.
		TArray<EAddOnArmorTestTechnique> CaptureTechniques;
		// Perf: the game view's camera; closer views put more skirt pixels on screen (shader cost).
		float ViewDistance = 5000.f;
		float ViewPitch = -60.f;
		// Perf: dump a ProfileGPU pass breakdown into the log halfway through the idle and hitting phases.
		bool bProfileGpu = false;
	};
	FRunSettings M_Settings;

	FVector M_BaseLocation = FVector::ZeroVector;
	float M_GroundHeight = 0.f;
	TArray<TWeakObjectPtr<AAddOnArmorTestVehicle>> M_Vehicles;
	TArray<FAddOnArmorCaptureCamera> M_Cameras;
	TWeakObjectPtr<ASkyLight> M_SkyFill;
	TWeakObjectPtr<ACPPController> M_PlayerController;
	bool bM_IsSuppressingErrorDialogs = false;

	FRandomStream M_HitStream;
	float M_HitBudget = 0.f;
	int32 M_HitSerial = 0;
	int32 M_HitsWithoutTarget = 0;

	// Capture: the scripted hits and the picture moments after each of them.
	TArray<FAddOnArmorScriptedHit> M_ScriptedHits;
	int32 M_NextScriptedHit = 0;
	// Game seconds into the hitting phase, and the picture's label.
	TArray<TPair<float, FString>> M_PictureMoments;
	int32 M_NextPictureMoment = 0;
	float M_LastScriptedHitSeconds = -1.f;
	FString M_LastScriptedHitName;

	// Perf: frame costs per phase, written at the end.
	TMap<FString, FAddOnArmorPerfPhaseStats> M_PhaseStats;
	FString M_FrameCsv;
	// Phases whose ProfileGPU dump was taken.
	TSet<EAddOnArmorBreakTestPhase> M_ProfiledPhases;
	void ProfileGpuHalfwayThrough(float PhaseSeconds);

	// Spike: what the landing and burst checks found so far.
	FAddOnArmorTestSpikeCounts M_SpikeCounts;
	// Spike: pictures to take, in absolute game seconds.
	TArray<TPair<float, FString>> M_SpikePictures;

	void TickWaitingForGameStart();
	void TickSettling();
	void TickWarmingUp();
	void TickIdle();
	void TickHitting(float DeltaTime);
	void TickSettlingDebris();

	void EnterPhase(EAddOnArmorBreakTestPhase NewPhase);
	double GetRealSecondsInPhase() const;
	float GetGameSecondsInPhase() const;
	float GetGameSeconds() const;
	static const TCHAR* GetPhaseName(EAddOnArmorBreakTestPhase Phase);

	bool ReadSettings();
	bool TryStartGame();
	void KeepGameRunning() const;
	bool FindBaseLocation();
	void SpawnSkyFill();
	bool SpawnVehicles();
	AAddOnArmorTestVehicle* SpawnVehicle(EAddOnArmorTestTechnique Technique, const FVector& Location, float Yaw);
	void CreateCameras();
	void AddCamera(const FString& Name, const FVector& LookAt, float Yaw, float Pitch, float Distance, float FieldOfView,
	               int32 Width, int32 Height);
	void AimCameras() const;
	void ExportCameras(const FString& Label) const;

	// Capture.
	void BuildScriptedHits();
	void TickCapture();
	void FireScriptedHit(const FAddOnArmorScriptedHit& ScriptedHit);

	// Perf and spike.
	void FireRandomHits(float DeltaTime);
	bool FireRandomHit();
	void RecordFrame();
	void WritePerfResults();
	static float GetAverage(const TArray<float>& Values);
	static float GetPercentile(TArray<float> Values, float Percentile);

	// Spike.
	void CheckSpikeReleases();
	void ExportDueSpikePictures();
	void ReportSpikeResult();

	void Finish();
};
