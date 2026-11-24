// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "RattleFeedTakeoverValidateSubsystem.generated.h"

class ACPPController;
class UAsyncShowRattleFeedTakeover;
class UPlayerStartGameControl;
class UW_RattleFeedTakeover;

enum class ERattleFeedTakeoverValidatePhase : uint8
{
	WaitingForHud,
	// The node was called while the start-game screen waits; the takeover must not open over it.
	WaitingBeforeStart,
	StartingGame,
	WatchingTakeover,
	// UNDERSTOOD was pressed; On Closed must have fired once with the game unpaused.
	CheckingClosed,
	Done
};

/**
 * @brief Headless check of the Rattle-Feed takeover, started with -RattleFeedTakeoverValidate (best with -game
 * -RenderOffscreen on a unit test map): calls the Show Rattle-Feed Takeover node before the start button, checks it
 * waits, screenshots the typing, two clip frames, the finished panel and the HUD after closing to
 * Saved/InfoTakeover/RattleFeed/<height>p/, checks that the clip atlas loaded, that the game paused and that the clip
 * keeps changing frames while it is paused, that On Closed fires once after UNDERSTOOD with the game
 * unpaused and that it still opens
 * (without the never-show box) and closes as shown while unit intel is turned off. Grep RATTLE_FEED_TAKEOVER. Restores the setting and quits.
 */
UCLASS()
class RTS_SURVIVAL_API URattleFeedTakeoverValidateSubsystem : public UTickableWorldSubsystem
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
	ERattleFeedTakeoverValidatePhase M_Phase = ERattleFeedTakeoverValidatePhase::WaitingForHud;
	double M_PhaseStartSeconds = 0.0;
	double M_OpenSeconds = 0.0;
	// Real seconds after opening at which the finished panel was captured.
	double M_DoneShotSeconds = 0.0;
	// Real seconds after opening at which the typewriter was first seen done; negative until then.
	double M_TypingDoneSeconds = -1.0;
	int32 M_ShotsTaken = 0;
	// Clip frames seen change while the takeover is open; the clip must play while the game is paused.
	int32 M_LastClipFrame = INDEX_NONE;
	int32 M_ClipFrameChanges = 0;
	int32 M_ClipShotsTaken = 0;
	int32 M_MainClosedCount = 0;
	int32 M_TurnedOffClosedCount = 0;
	int32 M_Failures = 0;
	bool bM_MainWasShown = false;
	bool bM_MainWasPausedWhenClosed = false;
	bool bM_TurnedOffWasShown = false;
	bool bM_HasOpened = false;
	bool bM_HasCapturedAfterClose = false;
	bool bM_OriginalSettingValue = true;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<UPlayerStartGameControl> M_StartGameControl;
	TWeakObjectPtr<UW_RattleFeedTakeover> M_WatchedTakeover;

	UPROPERTY()
	TObjectPtr<UAsyncShowRattleFeedTakeover> M_MainNode;

	UPROPERTY()
	TObjectPtr<UAsyncShowRattleFeedTakeover> M_TurnedOffNode;

	void EnterPhase(const ERattleFeedTakeoverValidatePhase NewPhase);
	double GetSecondsInPhase() const;

	void TickWaitingForHud();
	void TickWaitingBeforeStart();
	void TickStartingGame();
	void TickWatchingTakeover();
	void TickCheckingClosed();

	void CallMainNode();
	void StartWatching(UW_RattleFeedTakeover& Takeover);
	void CheckShowsWhileTurnedOff();
	void TrackClip(const UW_RattleFeedTakeover& Takeover, const double SecondsOpen);

	UFUNCTION()
	void HandleMainClosed(bool bWasShown);

	UFUNCTION()
	void HandleTurnedOffClosed(bool bWasShown);

	void SetTakeoversEnabled(const bool bEnabled) const;
	UW_RattleFeedTakeover* GetActiveRattleFeedTakeover() const;
	void DismissDifficultyPicker() const;
	void Capture(const FString& Label) const;
	void Fail(const FString& Reason);
	void Finish();
};
