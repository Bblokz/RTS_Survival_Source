// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "RTS_Survival/GameUI/LocationIntro/LocationIntroTypes.h"

#include "MissionLocationIntroSession.generated.h"

class ACPPController;
class UMissionBase;
class UPlayerCameraController;
class UW_LocationIntro;

/** Everything one location intro needs; built by UMissionBase::ShowLocationIntro from its pins. */
USTRUCT()
struct FMissionLocationIntroRequest
{
	GENERATED_BODY()

	FLocationIntroTexts Texts;
	FLocationIntroPresentation Presentation;
	// World-space move of the camera pawn over the whole intro.
	FVector CameraOffset = FVector::ZeroVector;
	bool bPanCamera = false;
	FLocationIntroPanSettings PanSettings;
	float DurationSeconds = 0.f;
	int32 ID = 0;
};

/** How a location intro session ended; decides what the mission reports to its Blueprint. */
enum class ELocationIntroSessionEnd : uint8
{
	// Played to the end.
	Finished,
	// The screen stayed busy (cinematic, info takeover, tech tree) for too long; the intro never showed.
	SkippedScreenBusy,
	// Ended early by the mission (failure, clean-up, a cinematic); the Blueprint is not told.
	Cancelled
};

/**
 * @brief Runs one mission location intro: waits until the screen is free, takes it over (HUD hidden, camera input
 * locked), drifts and turns the camera and drives the W_LocationIntro overlay, then hands the screen back and
 * reports to the mission. Camera and overlay share the real-time clock of this object's tick, so they stay in step.
 * Created and owned by UMissionBase::ShowLocationIntro; RequestCancel ends it early with the same clean-up.
 */
UCLASS()
class RTS_SURVIVAL_API UMissionLocationIntroSession : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/**
	 * @brief Queues the intro; it starts at once when the screen is free, else as soon as it frees up.
	 * @return False when the player controller or camera is missing; nothing is changed then.
	 */
	bool StartSession(UMissionBase* MissionOwner, const FMissionLocationIntroRequest& Request);

	// Ends the intro now and restores HUD, camera input and (when asked) the camera rotation.
	void RequestCancel();

	bool GetHasCompleted() const { return bM_HasCompleted; }

	// True once the intro has taken the screen (not while it waits for it).
	bool GetIsHoldingScreen() const { return bM_IsRunning; }

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

private:
	UPROPERTY()
	TWeakObjectPtr<UMissionBase> M_Mission;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TWeakObjectPtr<UPlayerCameraController> M_CameraController;

	UPROPERTY()
	TObjectPtr<UW_LocationIntro> M_Widget;

	FMissionLocationIntroRequest M_Request;

	FVector M_StartCameraLocation = FVector::ZeroVector;
	FRotator M_StartCameraRotation = FRotator::ZeroRotator;
	double M_StartRealSeconds = 0.0;
	// Unpaused game seconds spent waiting for the screen; past the limit the intro is skipped.
	float M_SecondsWaitedForScreen = 0.f;
	bool bM_IsWaitingForScreen = false;
	bool bM_HasCameraPose = false;
	bool bM_IsRunning = false;
	bool bM_HasCompleted = false;

	void TickWaitingForScreen(const float DeltaTime);
	void TickRunning();
	// Takes the screen when it is free. @return True when the intro now runs.
	bool TryTakeScreen();
	void CacheStartCameraPose();
	void UpdateCamera(const float ElapsedSeconds) const;
	FRotator GetPanRotationOffset(const float ElapsedSeconds) const;
	// Ends the intro: hands the screen back when it held it and reports EndReason to the mission.
	void CompleteSession(const ELocationIntroSessionEnd EndReason);
	void RestoreCameraRotationIfNeeded() const;
	float GetElapsedSeconds() const;

	bool GetIsValidPlayerController(const bool bReportError = true) const;
	bool GetIsValidCameraController(const bool bReportError = true) const;
	bool GetIsValidWidget(const bool bReportError = true) const;
};
