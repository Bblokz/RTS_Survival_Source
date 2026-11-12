// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "SpacedArmourTakeoverValidateSubsystem.generated.h"

class ACPPController;
class UAsyncShowSpacedArmourTakeover;
class UPlayerStartGameControl;
class UW_SpacedArmourTakeover;

enum class ESpacedArmourTakeoverValidatePhase : uint8
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
 * @brief Headless check of the spaced armour takeover, started with -SpacedArmourTakeoverValidate (best with -game
 * -RenderOffscreen on a unit test map): calls the Show Spaced Armour Takeover node before the start button, checks it
 * waits, screenshots the typing, the finished panel and the HUD after closing to Saved/InfoTakeover/SpacedArmour/,
 * checks that the plate render loaded and the game paused, that On Closed fires once after UNDERSTOOD with the game
 * unpaused and at once while the takeovers are off. Grep SPACED_ARMOUR_TAKEOVER. Restores the setting and quits.
 */
UCLASS()
class RTS_SURVIVAL_API USpacedArmourTakeoverValidateSubsystem : public UTickableWorldSubsystem
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
	ESpacedArmourTakeoverValidatePhase M_Phase = ESpacedArmourTakeoverValidatePhase::WaitingForHud;
	double M_PhaseStartSeconds = 0.0;
	double M_OpenSeconds = 0.0;
	// Real seconds after opening at which the finished panel was captured.
	double M_DoneShotSeconds = 0.0;
	// Real seconds after opening at which the typewriter was first seen done; negative until then.
	double M_TypingDoneSeconds = -1.0;
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
	TWeakObjectPtr<UW_SpacedArmourTakeover> M_WatchedTakeover;

	UPROPERTY()
	TObjectPtr<UAsyncShowSpacedArmourTakeover> M_MainNode;

	UPROPERTY()
	TObjectPtr<UAsyncShowSpacedArmourTakeover> M_TurnedOffNode;

	void EnterPhase(const ESpacedArmourTakeoverValidatePhase NewPhase);
	double GetSecondsInPhase() const;

	void TickWaitingForHud();
	void TickWaitingBeforeStart();
	void TickStartingGame();
	void TickWatchingTakeover();
	void TickCheckingClosed();

	void CallMainNode();
	void StartWatching(UW_SpacedArmourTakeover& Takeover);
	void CheckTurnedOffNode();

	UFUNCTION()
	void HandleMainClosed(bool bWasShown);

	UFUNCTION()
	void HandleTurnedOffClosed(bool bWasShown);

	void SetTakeoversEnabled(const bool bEnabled) const;
	UW_SpacedArmourTakeover* GetActiveSpacedArmourTakeover() const;
	void DismissDifficultyPicker() const;
	void Capture(const FString& Label) const;
	void Fail(const FString& Reason);
	void Finish();
};
