// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"

#include "ImpactVfxCaptureSubsystem.generated.h"

class ACPPController;
class ASceneCapture2D;
class ASkyLight;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

enum class EImpactVfxCapturePhase : uint8
{
	Inactive,
	WaitingForGameStart,
	Settling,
	// Plays the effect once unpictured so its materials compile before the pictured run.
	WarmingUp,
	ClearingWarmUp,
	Playing,
	Clearing,
	Done
};

/** A camera around the impact point, given as yaw/pitch/distance so it reads like the player's view. */
struct FImpactVfxCaptureView
{
	FString Name;
	float YawFromImpact = 0.f;
	float Pitch = -70.f;
	float Distance = 3000.f;
	float FieldOfView = 90.f;
	// Height above the impact point the camera looks at.
	float LookAtHeight = 0.f;
};

/** A scene capture per view; it renders every frame so exposure and temporal history settle before a picture. */
struct FImpactVfxCaptureCamera
{
	FImpactVfxCaptureView View;
	TWeakObjectPtr<ASceneCapture2D> Capture;
	TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;
};

/**
 * @brief Pictures impact Niagara systems in an offscreen game run so their look, size and particle counts can be
 * judged without the editor: plays each system from -ImpactVfxCapture=/Game/A+/Game/B at -ImpactVfxAt=X,Y (else
 * the map's BP_UnitImage_LandscapeAnchor or player start) next to tank and soldier sized blocks and saves scene
 * captures at set moments.
 * Tools/ImpactVfx has the runner and contact sheet step. Not created in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UImpactVfxCaptureSubsystem : public UTickableWorldSubsystem
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
	EImpactVfxCapturePhase M_Phase = EImpactVfxCapturePhase::Inactive;
	// Real seconds pace the start (the game boots paused); game seconds pace each effect.
	double M_PhaseStartRealSeconds = 0.0;
	float M_PhaseStartGameSeconds = 0.f;

	TArray<FString> M_SystemPaths;
	int32 M_SystemIndex = 0;
	FString M_OutputDirectory;
	float M_EffectScale = 1.f;
	// Game seconds after the spawn at which pictures are taken; ascending.
	TArray<float> M_CaptureMoments;
	int32 M_NextMomentIndex = 0;

	TArray<FImpactVfxCaptureCamera> M_Cameras;
	FVector M_ImpactLocation = FVector::ZeroVector;
	// Yaw the cameras are placed around; the anchor's yaw when the map has one.
	float M_ViewYaw = 0.f;
	TWeakObjectPtr<ASkyLight> M_SkyFill;
	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<UNiagaraComponent> M_EffectComponent;
	// Highest live particle count seen per emitter of the effect that plays, to judge its cost.
	TMap<FName, int32> M_MaxParticlesPerEmitter;
	int32 M_MaxTotalParticles = 0;
	bool bM_IsSuppressingErrorDialogs = false;

	void TickWaitingForGameStart();
	void TickSettling();
	void TickWarmingUp();
	void TickClearingWarmUp();
	void TickPlaying();
	void TickClearing();

	void EnterPhase(EImpactVfxCapturePhase NewPhase);
	double GetRealSecondsInPhase() const;
	float GetGameSecondsInPhase() const;
	float GetGameSeconds() const;

	bool LoadSystemPaths();
	void LoadCaptureMoments();
	bool TryStartGame();
	// The game start tutorial opens on a difficulty picker that pauses the game; picks Normal and unpauses.
	void KeepGameRunning() const;
	// The point to play the effects at, before it is dropped onto the ground.
	bool FindImpactBaseLocation(FVector& OutLocation);
	void DropImpactLocationToGround();
	void SpawnScaleReferences() const;
	void SpawnSkyFill();
	bool CreateCameras();
	void ConfigureCaptureComponent(USceneCaptureComponent2D& CaptureComponent, UTextureRenderTarget2D& RenderTarget,
	                               const FImpactVfxCaptureView& View) const;
	void AimCameras() const;

	bool SpawnCurrentEffect();
	void RecordParticleCounts();
	void LogEffectCost() const;
	void DestroyEffect();
	/**
	 * @brief Saves what every camera rendered last frame, so one moment is pictured from all sides.
	 * @param MomentLabel Part of the file name telling when the picture was taken, e.g. t0.30.
	 */
	void ExportCameras(const FString& MomentLabel) const;
	FString GetCurrentSystemName() const;
	void Finish();

};
