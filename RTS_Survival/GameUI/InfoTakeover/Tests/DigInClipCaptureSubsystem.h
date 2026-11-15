// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "DigInClipCaptureSubsystem.generated.h"

class ACPPController;
class AActor;
class ADigInWall;
class ASceneCapture2D;
class UHealthComponent;
class UTextureRenderTarget2D;

enum class EDigInClipPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForUnits,
	// The Panzer IV builds its sandbag wall; the T-26s hold fire.
	DiggingIn,
	// The wall stands; the T-26s attack and their shells hit the sandbags.
	UnderFire,
	Done
};

/** One recording camera: a scene capture, its render target and the folder its frames go to. */
USTRUCT()
struct FDigInClipCamera
{
	GENERATED_BODY()

	TWeakObjectPtr<ASceneCapture2D> Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	FString OutputDirectory;
};

/**
 * @brief Records the frames of the Dig In clip shown in the Dig In takeover, started with -DigInClipCapture (with -game
 * -RenderOffscreen -benchmark -fps=24): a player Panzer IV H digs in facing two parked enemy T-26s; once the sandbags stand
 * the T-26s attack it. Two cameras record every tick: one close on the tank (Saved/InfoTakeover/DigIn/Frames) and one
 * on the duel from the side (.../FramesWide), spliced by make_clip_atlas.py. Logs every hit on the wall and on the tank.
 * Optional -AbilityClipAt=X,Y picks the spot. Grep DIGIN_CLIP. Quits when done.
 */
UCLASS()
class RTS_SURVIVAL_API UDigInClipCaptureSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	EDigInClipPhase M_Phase = EDigInClipPhase::Inactive;
	double M_PhaseStartGameSeconds = 0.0;
	int32 M_FramesSaved = 0;
	bool bM_IsSuppressingErrorDialogs = false;
	FVector M_Spot = FVector::ZeroVector;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<AActor> M_Tank;
	TArray<TWeakObjectPtr<AActor>> M_Targets;
	TWeakObjectPtr<ADigInWall> M_Wall;

	// The wall's max health rises once a second while it is built; it is done when it stops rising.
	float M_LastWallMaxHealth = -1.f;
	double M_LastWallGrowthGameSeconds = 0.0;
	float M_LastWallHealth = -1.f;
	float M_LastTankHealth = -1.f;
	int32 M_WallHits = 0;
	int32 M_TankHits = 0;
	int32 M_TargetsDead = 0;
	double M_AllTargetsDeadGameSeconds = -1.0;
	bool bM_HasLoggedWallDeath = false;

	UPROPERTY()
	FDigInClipCamera M_CloseCamera;

	UPROPERTY()
	FDigInClipCamera M_WideCamera;

	void EnterPhase(const EDigInClipPhase NewPhase);
	double GetGameSeconds() const;
	double GetSecondsInPhase() const;

	void TickWaitingForGameStart();
	void TickWaitingForUnits();
	void TickDiggingIn();
	void TickUnderFire();

	bool SpawnUnits();
	bool CreateCameras();
	bool CreateCamera(const FVector& LookAt, const FRotator& Rotation, const float Distance, const TCHAR* FolderName,
	                  FDigInClipCamera& OutCamera) const;
	bool GetIsActorAlive(const TWeakObjectPtr<AActor>& Unit) const;
	int32 CountLivingTargets() const;
	void DisableTurretsOf(const AActor* Unit) const;
	void OrderDigIn();
	void OrderTargetAttack();
	ADigInWall* FindWall();
	UHealthComponent* GetWallHealth();
	UHealthComponent* GetTankHealth() const;
	void TrackHits();
	void SaveFrames();
	void Fail(const FString& Reason);
	void Finish(const bool bSucceeded);
};
