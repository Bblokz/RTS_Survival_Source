// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionCinematicTakeOverSession.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "MovieSceneSequencePlayer.h"
#include "RTS_Survival/GameUI/HoldToConfirm/W_HoldToSkip.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSInputModeDefaults.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace MissionCinematicTakeOverSessionInternal
{
	constexpr int32 SkipWidgetZOrder = 0;
}

bool UMissionCinematicTakeOverSession::StartSession(
	UMissionBase* MissionOwner,
	UMovieSceneSequencePlayer* SequencePlayer,
	const bool bAllowOptionalSkipping)
{
	if (not IsValid(MissionOwner))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to start cinematic takeover session because MissionOwner is invalid."));
		return false;
	}

	if (not IsValid(SequencePlayer))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to start cinematic takeover session because SequencePlayer is invalid."));
		return false;
	}

	M_Mission = MissionOwner;
	M_SequencePlayer = SequencePlayer;
	M_RuntimeState.bM_AllowsOptionalSkipping = bAllowOptionalSkipping;

	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(MissionOwner);
	if (not IsValid(PlayerController))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to start cinematic takeover session because PlayerController is invalid."));
		return false;
	}

	M_PlayerController = PlayerController;

	if (bAllowOptionalSkipping && not PlayerController->GetIsValidHoldToSkipWidgetClass())
	{
		return false;
	}

	if (not MissionOwner->OnCinematicTakeOverFromMission(true))
	{
		return false;
	}

	M_RuntimeState.bM_HasAppliedMissionTakeOver = true;
	PlayerController->SetActiveMissionCinematicTakeOverSession(this);
	BindSequencePlayerDelegates();

	if (not bAllowOptionalSkipping)
	{
		M_SequencePlayer->Play();
		return true;
	}

	if (CreateSkipWidget())
	{
		M_SequencePlayer->Play();
		return true;
	}

	PlayerController->ClearActiveMissionCinematicTakeOverSession(this);
	MissionOwner->OnCinematicTakeOverFromMission(false);
	M_RuntimeState.bM_HasAppliedMissionTakeOver = false;
	UnbindSequencePlayerDelegates();
	return false;
}

void UMissionCinematicTakeOverSession::RequestCancel(const EMissionCinematicTakeOverResult CancelResult)
{
	CompleteSession(CancelResult);
}

void UMissionCinematicTakeOverSession::HandleControllerEscapePressed()
{
	if (not M_RuntimeState.bM_AllowsOptionalSkipping || M_RuntimeState.bM_HasCompleted)
	{
		return;
	}

	if (not GetIsValidPlayerController(false) || not GetIsValidSkipWidget(false))
	{
		return;
	}

	ApplySkipWidgetInputMode();
	M_SkipWidget->SetFocus();
}

void UMissionCinematicTakeOverSession::HandleSequencePlaybackFinished()
{
	const EMissionCinematicTakeOverResult CompletionResult = M_RuntimeState.bM_SkipWasRequested
		? EMissionCinematicTakeOverResult::Skipped
		: EMissionCinematicTakeOverResult::FinishedNaturally;

	CompleteSession(CompletionResult);
}

void UMissionCinematicTakeOverSession::HandleSequencePlaybackStopped()
{
	const EMissionCinematicTakeOverResult CompletionResult = M_RuntimeState.bM_SkipWasRequested
		? EMissionCinematicTakeOverResult::Skipped
		: EMissionCinematicTakeOverResult::StoppedExternally;

	CompleteSession(CompletionResult);
}

void UMissionCinematicTakeOverSession::HandleSkipHoldCompleted()
{
	M_RuntimeState.bM_SkipWasRequested = true;

	if (GetIsValidSequencePlayer(false))
	{
		M_SequencePlayer->GoToEndAndStop();
	}

	CompleteSession(EMissionCinematicTakeOverResult::Skipped);
}

bool UMissionCinematicTakeOverSession::CreateSkipWidget()
{
	if (not GetIsValidPlayerController() || not M_PlayerController->GetIsValidHoldToSkipWidgetClass())
	{
		return false;
	}

	M_SkipWidget = CreateWidget<UW_HoldToSkip>(M_PlayerController.Get(), M_PlayerController->M_HoldToSkipWidgetClass);
	if (not GetIsValidSkipWidget())
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create cinematic skip widget."));
		return false;
	}

	M_SkipWidget->AddToViewport(MissionCinematicTakeOverSessionInternal::SkipWidgetZOrder);
	M_SkipWidget->OnHoldCompleted.AddDynamic(this, &ThisClass::HandleSkipHoldCompleted);
	ApplySkipWidgetInputMode();
	M_SkipWidget->SetFocus();
	M_RuntimeState.bM_UsesSkipWidgetInputMode = true;
	return true;
}

void UMissionCinematicTakeOverSession::RemoveSkipWidget()
{
	if (not GetIsValidSkipWidget(false))
	{
		return;
	}

	M_SkipWidget->OnHoldCompleted.RemoveDynamic(this, &ThisClass::HandleSkipHoldCompleted);
	M_SkipWidget->StopHold();
	M_SkipWidget->RemoveFromParent();
	M_SkipWidget = nullptr;
}

void UMissionCinematicTakeOverSession::BindSequencePlayerDelegates()
{
	if (not GetIsValidSequencePlayer())
	{
		return;
	}

	M_SequencePlayer->OnFinished.RemoveDynamic(this, &ThisClass::HandleSequencePlaybackFinished);
	M_SequencePlayer->OnFinished.AddDynamic(this, &ThisClass::HandleSequencePlaybackFinished);
	M_SequencePlayer->OnStop.RemoveDynamic(this, &ThisClass::HandleSequencePlaybackStopped);
	M_SequencePlayer->OnStop.AddDynamic(this, &ThisClass::HandleSequencePlaybackStopped);
}

void UMissionCinematicTakeOverSession::UnbindSequencePlayerDelegates()
{
	if (not GetIsValidSequencePlayer(false))
	{
		return;
	}

	M_SequencePlayer->OnFinished.RemoveDynamic(this, &ThisClass::HandleSequencePlaybackFinished);
	M_SequencePlayer->OnStop.RemoveDynamic(this, &ThisClass::HandleSequencePlaybackStopped);
}

void UMissionCinematicTakeOverSession::ApplySkipWidgetInputMode() const
{
	if (not GetIsValidPlayerController(false) || not GetIsValidSkipWidget(false))
	{
		return;
	}

	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(M_PlayerController.Get(), M_SkipWidget);
	M_PlayerController->bShowMouseCursor = true;
	M_PlayerController->bEnableClickEvents = true;
	M_PlayerController->bEnableMouseOverEvents = true;
}

void UMissionCinematicTakeOverSession::RestoreRegularInputModeIfNeeded() const
{
	if (not GetIsValidPlayerController(false))
	{
		return;
	}

	RTSInputModeDefaults::ApplyRegularGameInputMode(M_PlayerController.Get());
}

void UMissionCinematicTakeOverSession::CompleteSession(const EMissionCinematicTakeOverResult CompletionResult)
{
	if (M_RuntimeState.bM_HasCompleted)
	{
		return;
	}

	M_RuntimeState.bM_HasCompleted = true;
	M_RuntimeState.M_CompletionResult = CompletionResult;

	UnbindSequencePlayerDelegates();
	RemoveSkipWidget();

	if (GetIsValidPlayerController(false))
	{
		M_PlayerController->ClearActiveMissionCinematicTakeOverSession(this);
	}

	if (M_RuntimeState.bM_HasAppliedMissionTakeOver && GetIsValidMission(false))
	{
		M_Mission->OnCinematicTakeOverFromMission(false);
		M_RuntimeState.bM_HasAppliedMissionTakeOver = false;
	}

	RestoreRegularInputModeIfNeeded();
	OnCompleted.Broadcast();
	OnCompletedWithResult.Broadcast(CompletionResult);

	if (GetIsValidMission(false))
	{
		M_Mission->ClearActiveCinematicTakeOverSession(this);
	}
}

bool UMissionCinematicTakeOverSession::GetIsValidMission(const bool bReportError) const
{
	if (M_Mission.IsValid())
	{
		return true;
	}

	if (bReportError)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_Mission"),
			TEXT("UMissionCinematicTakeOverSession::GetIsValidMission"),
			this
		);
	}

	return false;
}

bool UMissionCinematicTakeOverSession::GetIsValidSequencePlayer(const bool bReportError) const
{
	if (M_SequencePlayer.IsValid())
	{
		return true;
	}

	if (bReportError)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SequencePlayer"),
			TEXT("UMissionCinematicTakeOverSession::GetIsValidSequencePlayer"),
			this
		);
	}

	return false;
}

bool UMissionCinematicTakeOverSession::GetIsValidPlayerController(const bool bReportError) const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}

	if (bReportError)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_PlayerController"),
			TEXT("UMissionCinematicTakeOverSession::GetIsValidPlayerController"),
			this
		);
	}

	return false;
}

bool UMissionCinematicTakeOverSession::GetIsValidSkipWidget(const bool bReportError) const
{
	if (IsValid(M_SkipWidget))
	{
		return true;
	}

	if (bReportError)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SkipWidget"),
			TEXT("UMissionCinematicTakeOverSession::GetIsValidSkipWidget"),
			this
		);
	}

	return false;
}
