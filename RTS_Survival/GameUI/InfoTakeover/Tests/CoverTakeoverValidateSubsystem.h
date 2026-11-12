// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "CoverTakeoverValidateSubsystem.generated.h"

class ACPPController;
class UAsyncShowCoverTakeover;
class UPlayerStartGameControl;
class UW_CoverTakeover;

enum class ECoverTakeoverValidatePhase : uint8
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
 * @brief Headless check of the cover takeover, started with -CoverTakeoverValidate (best with -game -RenderOffscreen
 * on a unit test map): calls the Show Cover Takeover node before the start button, checks it waits, screenshots the
 * typing, the finished panel and the HUD after closing to Saved/InfoTakeover/Cover/, checks that all three preview
 * screenshots loaded, that On Closed fires once after UNDERSTOOD with the game unpaused and at once while the takeovers
 * are off. Grep COVER_TAKEOVER. Restores the setting and quits.
 */
UCLASS()
class RTS_SURVIVAL_API UCoverTakeoverValidateSubsystem : public UTickableWorldSubsystem
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
	ECoverTakeoverValidatePhase M_Phase = ECoverTakeoverValidatePhase::WaitingForHud;
	double M_PhaseStartSeconds = 0.0;
	double M_OpenSeconds = 0.0;
	int32 M_ShotsTaken = 0;
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
	TWeakObjectPtr<UW_CoverTakeover> M_WatchedTakeover;

	UPROPERTY()
	TObjectPtr<UAsyncShowCoverTakeover> M_MainNode;

	UPROPERTY()
	TObjectPtr<UAsyncShowCoverTakeover> M_TurnedOffNode;

	void EnterPhase(const ECoverTakeoverValidatePhase NewPhase);
	double GetSecondsInPhase() const;

	void TickWaitingForHud();
	void TickWaitingBeforeStart();
	void TickStartingGame();
	void TickWatchingTakeover();
	void TickCheckingClosed();

	void CallMainNode();
	void StartWatching(UW_CoverTakeover& Takeover);
	void CheckTurnedOffNode();

	UFUNCTION()
	void HandleMainClosed(bool bWasShown);

	UFUNCTION()
	void HandleTurnedOffClosed(bool bWasShown);

	void SetTakeoversEnabled(const bool bEnabled) const;
	UW_CoverTakeover* GetActiveCoverTakeover() const;
	void DismissDifficultyPicker() const;
	void Capture(const FString& Label) const;
	void Fail(const FString& Reason);
	void Finish();
};
