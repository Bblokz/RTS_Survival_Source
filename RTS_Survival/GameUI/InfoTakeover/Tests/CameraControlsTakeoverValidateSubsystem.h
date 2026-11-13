// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "CameraControlsTakeoverValidateSubsystem.generated.h"

class ACPPController;
class UAsyncShowCameraControlsTakeover;
class UPlayerStartGameControl;
class UUserWidget;
class UW_CameraControlsTakeover;

enum class ECameraControlsValidatePhase : uint8
{
	WaitingForHud,
	// The node was called while the start-game screen waits; the takeover must not open over it.
	WaitingBeforeStart,
	StartingGame,
	// The node was called; waiting for the takeover to open, then capturing page one's animation frames.
	WatchingTakeover,
	// CONTINUE was pressed: the next page (camera panning, then marquee selection) must show and animate while still
	// paused; On Closed waits for UNDERSTOOD on the last page.
	WatchingLaterPage,
	// Understood was pressed; On Closed must have fired with the game unpaused.
	CheckingClosed,
	// Two requests at once; the first ticks "never show again", so the queued second must finish as not shown.
	WatchingDroppedQueue,
	Done
};

/** What the run learned about one node call. */
USTRUCT()
struct FCameraControlsValidateRequest
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAsyncShowCameraControlsTakeover> Action = nullptr;

	int32 ClosedCount = 0;
	bool bWasShown = false;
	bool bWasPausedWhenClosed = false;
};

/**
 * @brief Headless check of the camera controls takeover, started with -CameraControlsTakeoverValidate (best with -game
 * -RenderOffscreen on a unit test map): calls the Show Camera Controls Takeover node, screenshots the typing and eight
 * frames of each page's looping illustrations to Saved/InfoTakeover/CameraControls/, presses CONTINUE on every page and
 * UNDERSTOOD on the last, and checks that the illustrations animate while paused, that On Closed waits for the last page and fires once, at
 * once while the takeovers are off, and for a request dropped from the queue by "never show again". Grep
 * CAMERA_TAKEOVER. Restores the setting and quits.
 */
UCLASS()
class RTS_SURVIVAL_API UCameraControlsTakeoverValidateSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	ECameraControlsValidatePhase M_Phase = ECameraControlsValidatePhase::WaitingForHud;

	// PREVIOUS is pressed once, on the last page, before UNDERSTOOD.
	bool bM_HasPressedPrevious = false;

	// The glass hover check on page one: a card pane and the footer button hovered without a mouse.
	bool bM_HasStartedHoverCheck = false;
	bool bM_HasFinishedHoverCheck = false;
	bool bM_HasHoverMidSweepShot = false;
	bool bM_HasReleasedSweepFreeze = false;
	bool bM_HasCheckedHoverOn = false;
	double M_HoverCheckStartSeconds = 0.0;
	int32 M_HoverPaneIndex = INDEX_NONE;
	int32 M_HoverButtonIndex = INDEX_NONE;
	double M_PhaseStartSeconds = 0.0;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<UW_CameraControlsTakeover> M_WatchedTakeover;
	double M_WatchedOpenSeconds = 0.0;
	int32 M_NextFrameShot = 0;
	bool bM_HasTitleShot = false;
	bool bM_HasDroppedQueueTakeoverOpened = false;
	bool bM_HasCapturedAfterClose = false;

	// Per illustration of the page being watched: the animated values summed at every frame shot; they must change
	// while paused.
	TArray<TArray<float>> M_IllustrationSignatures;

	UPROPERTY()
	FCameraControlsValidateRequest M_MainRequest;

	UPROPERTY()
	TWeakObjectPtr<UPlayerStartGameControl> M_StartGameControl;

	UPROPERTY()
	FCameraControlsValidateRequest M_TurnedOffRequest;

	UPROPERTY()
	FCameraControlsValidateRequest M_FirstQueuedRequest;

	UPROPERTY()
	FCameraControlsValidateRequest M_DroppedRequest;

	int32 M_Failures = 0;
	bool bM_OriginalSettingValue = true;

	void EnterPhase(const ECameraControlsValidatePhase NewPhase);
	double GetSecondsInPhase() const;

	void TickWaitingForHud();
	void TickWaitingBeforeStart();
	void TickStartingGame();
	void CallMainRequest();

	/** @return The takeover that opened in this phase; null while waiting (fails and finishes after a timeout). */
	UW_CameraControlsTakeover* WaitForTakeoverToOpen(const TCHAR* WhatIsAwaited);
	void StartWatchingMainTakeover(UW_CameraControlsTakeover& Takeover);
	void StartWatchingPage(const UW_CameraControlsTakeover& Takeover);

	/**
	 * @brief Takes the typing shot and the eight animation frames of the page that shows.
	 * @param Takeover The open takeover.
	 * @return True once every shot of the page is taken and the closing delay passed.
	 */
	bool TickPageShots(UW_CameraControlsTakeover& Takeover);
	void TickWatchingTakeover();
	void TickWatchingLaterPage();
	void PressContinueToNextPage(UW_CameraControlsTakeover& Takeover);

	/** Checks the page lamps (one per page, the shown one lit) and that PREVIOUS shows from page two on. */
	void CheckPageControls(const UW_CameraControlsTakeover& Takeover, const TCHAR* Context);
	void PressPreviousOnLastPage(UW_CameraControlsTakeover& Takeover);

	/**
	 * @brief Hovers a card pane (with the light band held mid-way for one shot) and the footer button, checks that
	 * their Hover ramps up and back to 0, and captures the hovered page.
	 * @param Takeover The open takeover on page one.
	 * @return True once the check is done.
	 */
	bool TickHoverCheck(UW_CameraControlsTakeover& Takeover);
	void StartHoverCheck(UW_CameraControlsTakeover& Takeover);
	void CheckHoverValues(const UW_CameraControlsTakeover& Takeover, const bool bExpectHovered);
	static int32 FindGlassElement(const UW_CameraControlsTakeover& Takeover, const FString& NamePrefix);
	static bool GetIsLastPage(const UW_CameraControlsTakeover& Takeover);
	void TickCheckingClosed();
	void TickWatchingDroppedQueue();

	void CaptureAnimationFrame(const UW_CameraControlsTakeover& Takeover);
	void RecordIllustrationSignatures(const UW_CameraControlsTakeover& Takeover);
	FString GetShotPrefix(const UW_CameraControlsTakeover& Takeover) const;
	static float GetIllustrationSignature(const UUserWidget& Illustration);
	void CheckIllustrationsAnimated();
	void CloseTakeover(UW_CameraControlsTakeover& Takeover) const;

	void CheckTurnedOffRequest();
	void StartDroppedQueueCheck();

	UAsyncShowCameraControlsTakeover* StartRequest(FCameraControlsValidateRequest& Request);

	UFUNCTION()
	void HandleMainClosed(bool bWasShown);

	UFUNCTION()
	void HandleTurnedOffClosed(bool bWasShown);

	UFUNCTION()
	void HandleFirstQueuedClosed(bool bWasShown);

	UFUNCTION()
	void HandleDroppedClosed(bool bWasShown);

	void RecordClosed(FCameraControlsValidateRequest& Request, const bool bWasShown) const;
	void SetTakeoversEnabled(const bool bEnabled) const;
	UW_CameraControlsTakeover* GetActiveCameraTakeover() const;
	void DismissDifficultyPicker() const;
	void Capture(const FString& Label) const;
	void Fail(const FString& Reason);
	void Finish();
};
