// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "VulturesAtClipCaptureSubsystem.generated.h"

class ACPPController;
class AActor;
class ASceneCapture2D;
class ASquadController;
class UTextureRenderTarget2D;

enum class EVulturesAtClipPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForUnits,
	// The T-26 drives across in front of the Vultures; they throw once it is in range.
	WaitingForTankApproach,
	Recording,
	Done
};

/**
 * @brief Records the frames of the Vultures AT stick grenade clip shown in the Vultures AT takeover, started with
 * -VulturesAtClipCapture (with -game -RenderOffscreen -benchmark -fps=24 so every game tick is the same length): a
 * player Vulture squad throws its stick grenades at an enemy T-26 driving past, filmed from a close three-quarter
 * camera, every tick saved to Saved/InfoTakeover/VulturesAt/Frames (or -VulturesAtClipDir=). Optional
 * -VulturesAtClipAt=X,Y picks the spot. Grep VULTURES_AT_CLIP. Quits when done.
 */
UCLASS()
class RTS_SURVIVAL_API UVulturesAtClipCaptureSubsystem : public UTickableWorldSubsystem
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
	EVulturesAtClipPhase M_Phase = EVulturesAtClipPhase::Inactive;
	double M_PhaseStartGameSeconds = 0.0;
	// Game seconds at which the tank died; negative while it lives.
	double M_TankDeathGameSeconds = -1.0;
	int32 M_FramesSaved = 0;
	bool bM_IsSuppressingErrorDialogs = false;
	FVector M_Spot = FVector::ZeroVector;
	FString M_OutputDirectory;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<ASquadController> M_VulturesSquad;
	TWeakObjectPtr<AActor> M_Tank;
	TWeakObjectPtr<ASceneCapture2D> M_Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_RenderTarget;

	void EnterPhase(const EVulturesAtClipPhase NewPhase);
	double GetGameSeconds() const;
	double GetSecondsInPhase() const;

	void TickWaitingForGameStart();
	void TickWaitingForUnits();
	void TickWaitingForTankApproach();
	void TickRecording();

	bool SpawnUnits();
	bool CreateCamera();
	bool GetAreUnitsReady() const;
	bool GetIsTankAlive() const;
	void DisableTankTurrets() const;
	void OrderThrow();
	void SaveFrame();
	void Fail(const FString& Reason);
	void Finish(const bool bSucceeded);
};
