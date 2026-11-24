// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "OreTakeoversValidateSubsystem.generated.h"

class ACPPController;
class UAsyncShowCapturingTrucksTakeover;
class UAsyncShowOreManifestTakeover;
class UPlayerStartGameControl;
class UW_InfoTakeover;

enum class EOreTakeoversValidatePhase : uint8
{
	WaitingForHud,
	// The capturing trucks node was called while the start-game screen waits; it must not open over it.
	WaitingBeforeStart,
	StartingGame,
	WatchingCapturingTrucks,
	CheckingCapturingTrucksClosed,
	WatchingManifest,
	CheckingManifestClosed,
	Done
};

/** What the run saw of one takeover, from opening to its On Closed. */
struct FOreTakeoverWatch
{
	// Shot names and log lines, e.g. "CapturingTrucks".
	FString Name;
	TWeakObjectPtr<UW_InfoTakeover> Takeover;
	double OpenSeconds = 0.0;
	double TypingDoneSeconds = -1.0;
	double DoneShotSeconds = 0.0;
	int32 ShotsTaken = 0;
	int32 ClosedCount = 0;
	bool bHasOpened = false;
	bool bWasShown = false;
	bool bWasPausedWhenClosed = false;
	bool bHasCapturedAfterClose = false;
};

/**
 * @brief Headless check of the two Ore Mountains takeovers, started with -OreTakeoversValidate (best with -game
 * -RenderOffscreen on a unit test map). Calls Show Capturing Trucks Takeover before the start button and checks it
 * waits, then checks the styled Blueprint opened paused, that the diagram's countdown advances while paused, and
 * screenshots typing, diagram frames, the finished panel and the HUD after UNDERSTOOD (On Closed once, unpaused).
 * Then, with unit intel turned off, Show Ore Manifest Takeover must still open with its six lines and the stamp, and
 * close the same way. Shots go to Saved/InfoTakeover/OreTakeovers/<height>p/. Grep ORE_TAKEOVERS. Restores and quits.
 */
UCLASS()
class RTS_SURVIVAL_API UOreTakeoversValidateSubsystem : public UTickableWorldSubsystem
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
	EOreTakeoversValidatePhase M_Phase = EOreTakeoversValidatePhase::WaitingForHud;
	double M_PhaseStartSeconds = 0.0;
	int32 M_Failures = 0;
	bool bM_OriginalSettingValue = true;

	FOreTakeoverWatch M_CapturingTrucksWatch;
	FOreTakeoverWatch M_ManifestWatch;

	// The diagram's countdown values seen while the capturing trucks takeover held the game paused.
	TSet<int32> M_SeenCountdownSeconds;
	bool bM_SawSurrenderedTag = false;
	// One bit per diagram still already taken.
	int32 M_DiagramShotsTaken = 0;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<UPlayerStartGameControl> M_StartGameControl;

	UPROPERTY()
	TObjectPtr<UAsyncShowCapturingTrucksTakeover> M_CapturingTrucksNode;

	UPROPERTY()
	TObjectPtr<UAsyncShowOreManifestTakeover> M_ManifestNode;

	void EnterPhase(const EOreTakeoversValidatePhase NewPhase);
	double GetSecondsInPhase() const;

	void TickWaitingForHud();
	void TickWaitingBeforeStart();
	void TickStartingGame();
	void TickWatchingCapturingTrucks();
	void TickWatchingManifest();

	/**
	 * @brief Shared watch step: typing shot, then the finished shot once the typewriter is done, then UNDERSTOOD.
	 * @param Watch The takeover being watched.
	 * @param Active The takeover open now (null when none).
	 * @return True once UNDERSTOOD was pressed; the caller moves on to its closed check.
	 */
	bool TickWatchStep(FOreTakeoverWatch& Watch, UW_InfoTakeover* Active);

	/**
	 * @brief Shared closed check: On Closed once, shown, unpaused; screenshots the HUD.
	 * @param Watch The takeover that was closed.
	 * @return True when the check ran and the caller can move on.
	 */
	bool TickClosedStep(FOreTakeoverWatch& Watch);

	void CallCapturingTrucksNode();
	void CallManifestNode();
	void CheckCapturingTrucksOpened(UW_InfoTakeover& Takeover);
	void CheckManifestOpened(UW_InfoTakeover& Takeover);
	void SampleDiagram();

	UFUNCTION()
	void HandleCapturingTrucksClosed(bool bWasShown);

	UFUNCTION()
	void HandleManifestClosed(bool bWasShown);

	void RecordClosed(FOreTakeoverWatch& Watch, const bool bWasShown);
	void SetTakeoversEnabled(const bool bEnabled) const;
	UW_InfoTakeover* GetActiveTakeover() const;
	void DismissDifficultyPicker() const;
	void Capture(const FString& Label) const;
	void Fail(const FString& Reason);
	void Finish();
};
