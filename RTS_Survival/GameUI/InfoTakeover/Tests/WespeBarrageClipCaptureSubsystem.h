// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "WespeBarrageClipCaptureSubsystem.generated.h"

class ACPPController;
class AActor;
class ASceneCapture2D;
class UTextureRenderTarget2D;
class UWeaponState;

enum class EWespeBarrageClipPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForUnits,
	// The barrage is ordered; both cameras record until it ends.
	Recording,
	Done
};

/** One recording camera: a scene capture, its render target and the folder its frames go to. */
USTRUCT()
struct FWespeBarrageClipCamera
{
	GENERATED_BODY()

	TWeakObjectPtr<ASceneCapture2D> Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	FString OutputDirectory;
};

/**
 * @brief Records the frames of the Wespe Barrage clip shown in the Wespe Barrage takeover, started with
 * -WespeBarrageClipCapture (with -game -RenderOffscreen -benchmark -fps=24): a battery of three player Wespes uses the
 * Barrage aim ability on an enemy T-26 and Mosin squad far out, which a player Panzer II spots. Two cameras record every tick: one on the battery
 * (Saved/InfoTakeover/WespeBarrage/Frames) and one on the target area (.../FramesTarget), spliced by
 * make_clip_atlas.py. Optional -AbilityClipAt=X,Y picks the battery spot. Grep WESPE_BARRAGE_CLIP. Quits when done.
 */
UCLASS()
class RTS_SURVIVAL_API UWespeBarrageClipCaptureSubsystem : public UTickableWorldSubsystem
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
	EWespeBarrageClipPhase M_Phase = EWespeBarrageClipPhase::Inactive;
	double M_PhaseStartGameSeconds = 0.0;
	int32 M_FramesSaved = 0;
	bool bM_IsSuppressingErrorDialogs = false;
	FVector M_Spot = FVector::ZeroVector;
	FVector M_TargetCenter = FVector::ZeroVector;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TArray<TWeakObjectPtr<AActor>> M_Wespes;
	TArray<TWeakObjectPtr<AActor>> M_Targets;
	// Player tank that gives vision on the target area; see SpotterTank in the cpp.
	TWeakObjectPtr<AActor> M_Spotter;
	// Rounds in each Wespe's howitzer last tick, by Wespe index; a drop is a shot.
	TArray<int32> M_LastHowitzerRounds;
	// Saved-frame index of every howitzer shot of the battery.
	TArray<int32> M_ShotFrames;

	UPROPERTY()
	FWespeBarrageClipCamera M_BatteryCamera;

	UPROPERTY()
	FWespeBarrageClipCamera M_TargetCamera;

	void EnterPhase(const EWespeBarrageClipPhase NewPhase);
	double GetGameSeconds() const;
	double GetSecondsInPhase() const;

	void TickWaitingForGameStart();
	void TickWaitingForUnits();
	void TickRecording();

	bool SpawnUnits();
	bool CreateCameras();
	bool CreateCamera(const FVector& LookAt, const FRotator& Rotation, const float Distance, const TCHAR* FolderName,
	                  FWespeBarrageClipCamera& OutCamera) const;
	bool GetAreUnitsReady() const;
	int32 CountLivingTargets() const;
	void DisableTargetTurrets() const;
	void HoldBatteryFire() const;
	void DisableTurretsOf(const AActor* Unit) const;
	void OrderBarrage();
	UWeaponState* FindHowitzer(const AActor* Wespe) const;
	void TrackShots();
	void SaveFrames();
	void Fail(const FString& Reason);
	void Finish(const bool bSucceeded);
};
