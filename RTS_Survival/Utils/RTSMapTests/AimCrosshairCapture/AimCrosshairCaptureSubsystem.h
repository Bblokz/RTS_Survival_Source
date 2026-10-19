// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "RTS_Survival/Player/PlayerAimAbilitiy/PlayerAimAbilityTypes/PlayerAimAbilityTypes.h"

#include "AimCrosshairCaptureSubsystem.generated.h"

class ACPPController;
class APlayerAimAbility;
class ASceneCapture2D;
class ASkyLight;
class UMaterialInterface;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

enum class EAimCrosshairCapturePhase : uint8
{
	Inactive,
	WaitingForGameStart,
	Settling,
	// Shows every case once unpictured, so freshly imported textures and shaders are built before the pictures.
	WarmingUp,
	ShowingCase,
	Done
};

/** One reticle to picture: the aim type and radius an ability would pass, optionally with a material override. */
struct FAimCrosshairCaptureCase
{
	FString Label;
	EPlayerAimAbilityTypes AimType = EPlayerAimAbilityTypes::None;
	float Radius = 0.f;
	// Replaces the material the aim actor picked, for reticles whose aim type is not mapped on the Blueprint yet.
	FString MaterialOverridePath;
};

/** A camera looking at the reticle, given like the player's spring arm: pitch, arm length and field of view. */
struct FAimCrosshairCaptureView
{
	FString Name;
	float Pitch = -70.f;
	float Distance = 3000.f;
	float FieldOfView = 90.f;
};

struct FAimCrosshairCaptureCamera
{
	FAimCrosshairCaptureView View;
	TWeakObjectPtr<ASceneCapture2D> Capture;
	TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;
};

/**
 * @brief Pictures the aim ability reticles in an offscreen game run so their textures and material can be judged
 * from the real game camera: spawns the aim actor Blueprint, shows each case through
 * APlayerAimAbility::ShowGlobalAbilityRadius (the path abilities use) and saves scene captures per view.
 * Started with -AimCrosshairCapture; Tools/AimCrosshairs has the runner and contact sheets. Not created in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UAimCrosshairCaptureSubsystem : public UTickableWorldSubsystem
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
	EAimCrosshairCapturePhase M_Phase = EAimCrosshairCapturePhase::Inactive;
	// Real seconds pace the start (the game boots paused); game seconds pace each case.
	double M_PhaseStartRealSeconds = 0.0;
	float M_PhaseStartGameSeconds = 0.f;

	TArray<FAimCrosshairCaptureCase> M_Cases;
	int32 M_CaseIndex = 0;
	// Ground points to picture every case at, e.g. grass and a bright field.
	TArray<FVector> M_Spots;
	int32 M_SpotIndex = 0;
	FString M_OutputDirectory;
	// Yaw the cameras look along; the aim actor faces the same way so its direction marks point up on screen.
	float M_ViewYaw = 0.f;
	float M_AimYawOffset = 0.f;

	TArray<FAimCrosshairCaptureCamera> M_Cameras;
	UPROPERTY()
	TObjectPtr<APlayerAimAbility> M_AimActor;
	TWeakObjectPtr<ASkyLight> M_SkyFill;
	TWeakObjectPtr<ACPPController> M_PlayerController;
	bool bM_IsSuppressingErrorDialogs = false;

	void TickWaitingForGameStart();
	void TickSettling();
	void TickWarmingUp();
	void FinishPendingCompilation() const;
	void TickShowingCase();

	void EnterPhase(EAimCrosshairCapturePhase NewPhase);
	double GetRealSecondsInPhase() const;
	float GetGameSecondsInPhase() const;
	float GetGameSeconds() const;

	void LoadCases();
	bool TryParseCase(const FString& CaseText, FAimCrosshairCaptureCase& OutCase) const;
	bool LoadSpots();
	bool TryStartGame();
	// The game start tutorial opens on a difficulty picker that pauses the game; picks Normal and unpauses.
	void KeepGameRunning() const;
	FVector DropToGround(const FVector& Location) const;
	void SpawnScaleReferences(const FVector& Spot) const;
	void SpawnSkyFill();
	bool SpawnAimActor();
	bool GetIsValidAimActor() const;
	// With -AimCrosshairWireUnitsPerScale, gives the spawned aim actor the new crosshair materials before the
	// Blueprint itself is wired.
	void ApplyTestWiring() const;
	bool CreateCameras();
	void ConfigureCaptureComponent(USceneCaptureComponent2D& CaptureComponent, UTextureRenderTarget2D& RenderTarget,
	                               const FAimCrosshairCaptureView& View) const;
	void AimCameras() const;

	/** @return False when the aim actor refused the case (its error is in the log). */
	bool ShowCurrentCase();
	void ApplyMaterialOverride(const FAimCrosshairCaptureCase& CaptureCase) const;
	void ExportCameras() const;
	// Shows the current case, skipping refused ones; false when none is left.
	bool ShowFirstShowableCase();
	/** Moves to the next case, then the next spot; @return False when every case was pictured at every spot. */
	bool AdvanceCase();
	void Finish();
};
