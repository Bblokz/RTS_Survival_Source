// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "BxpPackUpTakeoverValidateSubsystem.generated.h"

class ACPPController;
class UAsyncShowBxpPackUpTakeover;
class UPlayerStartGameControl;
class UUserWidget;
class UW_BxpPackUpTakeover;

enum class EBxpPackUpTakeoverValidatePhase : uint8
{
	WaitingForHud,
	// The node was called while the start-game screen waits; the takeover must not open over it.
	WaitingBeforeStart,
	StartingGame,
	// Typing shot, animation frames (the illustration must move while paused), then the finished panel.
	WatchingTakeover,
	// UNDERSTOOD was pressed; On Closed must have fired once with the game unpaused.
	CheckingClosed,
	Done
};

/**
 * @brief Headless check of the building expansion pack-up takeover, started with -BxpPackUpTakeoverValidate (best with
 * -game -RenderOffscreen on a unit test map): calls the Show Building Expansion Pack Up Takeover node before the start
 * button, checks it waits, screenshots the typing, eight frames of the looping illustration, the finished panel and the
 * HUD after closing to Saved/InfoTakeover/BxpPackUp/, checks that the illustration animates while the game is paused,
 * that On Closed fires once after UNDERSTOOD with the game unpaused and that it still opens
 * (without the never-show box) and closes as shown while unit intel is turned off. Grep
 * BXP_PACKUP_TAKEOVER. Restores the setting and quits.
 */
UCLASS()
class RTS_SURVIVAL_API UBxpPackUpTakeoverValidateSubsystem : public UTickableWorldSubsystem
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
	EBxpPackUpTakeoverValidatePhase M_Phase = EBxpPackUpTakeoverValidatePhase::WaitingForHud;
	double M_PhaseStartSeconds = 0.0;
	double M_OpenSeconds = 0.0;
	// Real seconds after opening at which the typewriter was first seen done; negative until then.
	double M_TypingDoneSeconds = -1.0;
	// Real seconds after opening at which the finished panel was captured; negative until then.
	double M_DoneShotSeconds = -1.0;
	int32 M_NextFrameShot = 0;
	int32 M_NextBeatShot = 0;
	// Real seconds after opening of the last frame or beat shot.
	double M_LastShotSeconds = 0.0;
	// GFrameCounter when the loop was jumped to the next beat; INDEX_NONE while no beat waits for its shot.
	int64 M_BeatJumpFrame = INDEX_NONE;
	int32 M_MainClosedCount = 0;
	int32 M_TurnedOffClosedCount = 0;
	int32 M_Failures = 0;
	bool bM_HasTypingShot = false;
	bool bM_MainWasShown = false;
	bool bM_MainWasPausedWhenClosed = false;
	bool bM_TurnedOffWasShown = false;
	bool bM_HasOpened = false;
	bool bM_HasCapturedAfterClose = false;
	bool bM_OriginalSettingValue = true;

	// The illustration's animated values summed at every frame shot; they must change while paused.
	TArray<float> M_IllustrationSignatures;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<UPlayerStartGameControl> M_StartGameControl;
	TWeakObjectPtr<UW_BxpPackUpTakeover> M_WatchedTakeover;

	UPROPERTY()
	TObjectPtr<UAsyncShowBxpPackUpTakeover> M_MainNode;

	UPROPERTY()
	TObjectPtr<UAsyncShowBxpPackUpTakeover> M_TurnedOffNode;

	void EnterPhase(const EBxpPackUpTakeoverValidatePhase NewPhase);
	double GetSecondsInPhase() const;

	void TickWaitingForHud();
	void TickWaitingBeforeStart();
	void TickStartingGame();
	void TickWatchingTakeover();
	void TickCheckingClosed();

	void CallMainNode();
	void StartWatching(UW_BxpPackUpTakeover& Takeover);

	/**
	 * @brief Takes the typing shot and the animation frames while the takeover is open.
	 * @param Takeover The open takeover.
	 * @param SecondsOpen Real seconds since it opened.
	 * @return True once every frame shot is taken.
	 */
	bool TickFrameShots(const UW_BxpPackUpTakeover& Takeover, const double SecondsOpen);

	/**
	 * @brief Captures the finished panel once the typewriter is done (or after the longest wait).
	 * @param Takeover The open takeover.
	 * @param SecondsOpen Real seconds since it opened.
	 * @return True once the finished panel was captured.
	 */
	bool TickDoneShot(const UW_BxpPackUpTakeover& Takeover, const double SecondsOpen);

	void CaptureAnimationFrame(const UW_BxpPackUpTakeover& Takeover);

	/**
	 * @brief Restarts the illustration's loop at one beat; its shot is taken a few frames later, so every beat can be
	 * reviewed.
	 * @param Takeover The open takeover.
	 * @param AnimationSeconds The time in the loop to show.
	 */
	void JumpToBeat(const UW_BxpPackUpTakeover& Takeover, const float AnimationSeconds);
	static float GetIllustrationSignature(const UUserWidget& Illustration);
	void CheckIllustrationAnimated();
	void CheckShowsWhileTurnedOff();

	UFUNCTION()
	void HandleMainClosed(bool bWasShown);

	UFUNCTION()
	void HandleTurnedOffClosed(bool bWasShown);

	void SetTakeoversEnabled(const bool bEnabled) const;
	UW_BxpPackUpTakeover* GetActiveBxpPackUpTakeover() const;
	void DismissDifficultyPicker() const;
	void Capture(const FString& Label) const;
	void Fail(const FString& Reason);
	void Finish();
};
