// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MissionCinematicTakeOverSession.generated.h"

class ACPPController;
class UMissionBase;
class UMovieSceneSequencePlayer;
class UW_HoldToSkip;

UENUM(BlueprintType)
enum class EMissionCinematicTakeOverResult : uint8
{
	NotCompleted,
	FinishedNaturally,
	Skipped,
	StoppedExternally,
	Cancelled
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMissionCinematicTakeOverCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FMissionCinematicTakeOverCompletedWithResult,
	EMissionCinematicTakeOverResult,
	Result);

USTRUCT()
struct FMissionCinematicTakeOverSessionRuntimeState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bM_HasAppliedMissionTakeOver = false;

	UPROPERTY()
	bool bM_AllowsOptionalSkipping = false;

	UPROPERTY()
	bool bM_UsesSkipWidgetInputMode = false;

	UPROPERTY()
	bool bM_SkipWasRequested = false;

	UPROPERTY()
	bool bM_HasCompleted = false;

	UPROPERTY()
	EMissionCinematicTakeOverResult M_CompletionResult = EMissionCinematicTakeOverResult::NotCompleted;
};

	/**
	 * @brief Mission-owned runtime handle for a cinematic takeover.
	 * Blueprints keep the returned handle to react once the sequence ends, stops, or gets skipped.
	 * The session starts playback after takeover state and delegate bindings are fully configured.
	 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UMissionCinematicTakeOverSession : public UObject
{
	GENERATED_BODY()

public:
	bool StartSession(
		UMissionBase* MissionOwner,
		UMovieSceneSequencePlayer* SequencePlayer,
		const bool bAllowOptionalSkipping);

	void RequestCancel(
		const EMissionCinematicTakeOverResult CancelResult = EMissionCinematicTakeOverResult::Cancelled);

	void HandleControllerEscapePressed();

	UFUNCTION(BlueprintPure, Category = "Cinematics")
	bool GetHasCompleted() const
	{
		return M_RuntimeState.bM_HasCompleted;
	}

	UFUNCTION(BlueprintPure, Category = "Cinematics")
	EMissionCinematicTakeOverResult GetCompletionResult() const
	{
		return M_RuntimeState.M_CompletionResult;
	}

	UPROPERTY(BlueprintAssignable, Category = "Cinematics")
	FMissionCinematicTakeOverCompleted OnCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Cinematics")
	FMissionCinematicTakeOverCompletedWithResult OnCompletedWithResult;

private:
	UFUNCTION()
	void HandleSequencePlaybackFinished();

	UFUNCTION()
	void HandleSequencePlaybackStopped();

	UFUNCTION()
	void HandleSkipHoldCompleted();

	bool CreateSkipWidget();
	void RemoveSkipWidget();
	void BindSequencePlayerDelegates();
	void UnbindSequencePlayerDelegates();
	void ApplySkipWidgetInputMode() const;
	void RestoreRegularInputModeIfNeeded() const;
	void CompleteSession(const EMissionCinematicTakeOverResult CompletionResult);

	bool GetIsValidMission(const bool bReportError = true) const;
	bool GetIsValidSequencePlayer(const bool bReportError = true) const;
	bool GetIsValidPlayerController(const bool bReportError = true) const;
	bool GetIsValidSkipWidget(const bool bReportError = true) const;

private:
	UPROPERTY()
	TWeakObjectPtr<UMissionBase> M_Mission;

	UPROPERTY()
	TWeakObjectPtr<UMovieSceneSequencePlayer> M_SequencePlayer;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TObjectPtr<UW_HoldToSkip> M_SkipWidget = nullptr;

	UPROPERTY(Transient)
	FMissionCinematicTakeOverSessionRuntimeState M_RuntimeState;
};
