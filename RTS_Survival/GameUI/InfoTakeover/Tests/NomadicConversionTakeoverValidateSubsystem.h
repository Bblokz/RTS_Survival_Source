// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "NomadicConversionTakeoverValidateSubsystem.generated.h"

class ACPPController;
class UAsyncShowNomadicConversionTakeover;
class UPlayerStartGameControl;
class UUserWidget;
class UW_NomadicConversionTakeover;

enum class ENomadicConversionTakeoverValidatePhase : uint8
{
	WaitingForHud,
	// The node was called while the start-game screen waits; the takeover must not open over it.
	WaitingBeforeStart,
	StartingGame,
	// Per page: typing shot, animation frames (the illustration must move while paused), then the finished panel;
	// CONTINUE goes from the conversion page to the build radius page.
	WatchingTakeover,
	// UNDERSTOOD was pressed; On Closed must have fired once with the game unpaused.
	CheckingClosed,
	Done
};

/**
 * @brief Headless check of the nomadic conversion takeover, started with -NomadicConversionTakeoverValidate (best with
 * -game -RenderOffscreen on a unit test map): calls the Show Nomadic Conversion Takeover node before the start button,
 * checks it waits, screenshots per page (conversion, then build radius after CONTINUE) the typing, eight frames of the
 * looping illustration and the finished panel, and the HUD after closing to Saved/InfoTakeover/NomadicConversion/;
 * checks that each illustration animates while the game is paused, the page lamps and PREVIOUS on the second page,
 * that On Closed fires once after UNDERSTOOD with the game unpaused and that it still opens
 * (without the never-show box) and closes as shown while unit intel is turned off. Grep
 * NOMADIC_TAKEOVER. Restores the setting and quits.
 */
UCLASS()
class RTS_SURVIVAL_API UNomadicConversionTakeoverValidateSubsystem : public UTickableWorldSubsystem
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
	ENomadicConversionTakeoverValidatePhase M_Phase = ENomadicConversionTakeoverValidatePhase::WaitingForHud;
	double M_PhaseStartSeconds = 0.0;
	// Real seconds at which the watched page opened (the takeover itself, or CONTINUE for the second page).
	double M_OpenSeconds = 0.0;
	// Real seconds after opening at which the typewriter was first seen done; negative until then.
	double M_TypingDoneSeconds = -1.0;
	// Real seconds after opening at which the finished panel was captured; negative until then.
	double M_DoneShotSeconds = -1.0;
	int32 M_NextFrameShot = 0;
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
	TWeakObjectPtr<UW_NomadicConversionTakeover> M_WatchedTakeover;

	UPROPERTY()
	TObjectPtr<UAsyncShowNomadicConversionTakeover> M_MainNode;

	UPROPERTY()
	TObjectPtr<UAsyncShowNomadicConversionTakeover> M_TurnedOffNode;

	void EnterPhase(const ENomadicConversionTakeoverValidatePhase NewPhase);
	double GetSecondsInPhase() const;

	void TickWaitingForHud();
	void TickWaitingBeforeStart();
	void TickStartingGame();
	void TickWatchingTakeover();
	void TickCheckingClosed();

	void CallMainNode();
	void StartWatching(UW_NomadicConversionTakeover& Takeover);

	/** CONTINUE from the conversion page: checks the build radius page shows, then watches it from the start. */
	void ContinueToBuildRadiusPage(UW_NomadicConversionTakeover& Takeover);

	/** @return The watched page's illustration; null when it did not load. */
	static const UUserWidget* GetActivePageIllustration(const UW_NomadicConversionTakeover& Takeover);

	/** @return The screenshot name prefix of the watched page. */
	static FString GetPageShotPrefix(const UW_NomadicConversionTakeover& Takeover);

	/**
	 * @brief Takes the typing shot and the animation frames while the takeover is open.
	 * @param Takeover The open takeover.
	 * @param SecondsOpen Real seconds since it opened.
	 * @return True once every frame shot is taken.
	 */
	bool TickFrameShots(const UW_NomadicConversionTakeover& Takeover, const double SecondsOpen);

	/**
	 * @brief Captures the finished panel once the typewriter is done (or after the longest wait).
	 * @param Takeover The open takeover.
	 * @param SecondsOpen Real seconds since it opened.
	 * @return True once the finished panel was captured.
	 */
	bool TickDoneShot(const UW_NomadicConversionTakeover& Takeover, const double SecondsOpen);

	void CaptureAnimationFrame(const UW_NomadicConversionTakeover& Takeover);
	static float GetIllustrationSignature(const UUserWidget& Illustration);
	void CheckIllustrationAnimated();
	void CheckShowsWhileTurnedOff();

	UFUNCTION()
	void HandleMainClosed(bool bWasShown);

	UFUNCTION()
	void HandleTurnedOffClosed(bool bWasShown);

	void SetTakeoversEnabled(const bool bEnabled) const;
	UW_NomadicConversionTakeover* GetActiveNomadicConversionTakeover() const;
	void DismissDifficultyPicker() const;
	void Capture(const FString& Label) const;
	void Fail(const FString& Reason);
	void Finish();
};
