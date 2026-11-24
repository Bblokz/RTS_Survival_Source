// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PortraitManager.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/GameUI/Portrait/PortraitWidget/W_Portrait.h"
#include "RTS_Survival/Player/PlayerAudioController/PlayerAudioController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UPlayerPortraitManager::UPlayerPortraitManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPlayerPortraitManager::InitPortraitManager(
	UW_Portrait* InPortraitWidget,
	UPlayerAudioController* AudioControllerForVoiceLines)
{
	M_PortraitWidget = InPortraitWidget;
	M_AudioController = AudioControllerForVoiceLines;

	// Ensure we start in a hidden state if the widget is valid.
	if (GetIsValidPortraitWidget())
	{
		M_PortraitWidget->HidePortrait();
	}
}

void UPlayerPortraitManager::PlayPortrait(
	const ERTSPortraitTypes PortraitType,
	USoundBase* const VoiceLine)
{
	if (PortraitType == ERTSPortraitTypes::None || not IsValid(VoiceLine))
	{
		RTSFunctionLibrary::ReportError(
			"UPlayerPortraitManager::PlayPortrait received invalid portrait type or voice line.");
		return;
	}

	if (not GetIsValidAudioController())
	{
		return;
	}

	EnqueuePortraitRequest(PortraitType, VoiceLine);

	if (bM_IsPortraitPlaying)
	{
		return;
	}

	StartNextPortraitFromQueue();
}

void UPlayerPortraitManager::BeginPlay()
{
	Super::BeginPlay();
}

void UPlayerPortraitManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Without a portrait widget (the main menu controller never gets one) no portrait can be playing.
	if (M_PortraitWidget.IsValid())
	{
		StopCurrentPortraitPlayback();
	}
	Super::EndPlay(EndPlayReason);
}

void UPlayerPortraitManager::StopCurrentPortraitPlayback()
{
	M_PortraitQueue.Reset();
	bM_IsPortraitPlaying = false;
	M_CurrentRequest = FPortraitQueueEntry();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CurrentPortraitTimerHandle);
	}

	if (M_CinematicHostWidget.IsValid())
	{
		M_CinematicHostWidget->HidePortrait();
	}
	if (not GetIsValidPortraitWidget())
	{
		return;
	}

	M_PortraitWidget->HidePortrait();
}

void UPlayerPortraitManager::SetCinematicPortraitHost(UW_Portrait* HostWidget)
{
	if (not IsValid(HostWidget))
	{
		RTSFunctionLibrary::ReportError("UPlayerPortraitManager::SetCinematicPortraitHost got no widget.");
		return;
	}
	UW_Portrait* const PreviousWidget = GetActivePortraitWidget();
	M_CinematicHostWidget = HostWidget;
	MovePlayingPortrait(PreviousWidget, HostWidget);
}

void UPlayerPortraitManager::ClearCinematicPortraitHost(const UW_Portrait* HostWidget)
{
	if (not M_CinematicHostWidget.IsValid() || M_CinematicHostWidget.Get() != HostWidget)
	{
		return;
	}
	UW_Portrait* const PreviousHost = M_CinematicHostWidget.Get();
	M_CinematicHostWidget = nullptr;
	MovePlayingPortrait(PreviousHost, M_PortraitWidget.Get());
}

UW_Portrait* UPlayerPortraitManager::GetActivePortraitWidget() const
{
	return M_CinematicHostWidget.IsValid() ? M_CinematicHostWidget.Get() : M_PortraitWidget.Get();
}

void UPlayerPortraitManager::MovePlayingPortrait(UW_Portrait* From, UW_Portrait* To) const
{
	if (From == To)
	{
		return;
	}
	if (IsValid(From))
	{
		From->HidePortrait();
	}
	if (not bM_IsPortraitPlaying || not IsValid(To) || not M_CurrentRequest.IsValid())
	{
		return;
	}
	To->UpdatePortrait(M_CurrentRequest.PortraitType);
	StartSubtitlesForRequest(M_CurrentRequest, *To);
}

bool UPlayerPortraitManager::GetIsValidPortraitWidget() const
{
	if (M_PortraitWidget.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		"UPlayerPortraitManager has no valid portrait widget.\n"
		"See UPlayerPortraitManager::GetIsValidPortraitWidget() for more details.");
	return false;
}

bool UPlayerPortraitManager::GetIsValidActivePortraitWidget() const
{
	if (M_CinematicHostWidget.IsValid())
	{
		return true;
	}
	return GetIsValidPortraitWidget();
}

bool UPlayerPortraitManager::GetIsValidAudioController() const
{
	if (M_AudioController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		"UPlayerPortraitManager has no valid PlayerAudioController.\n"
		"See UPlayerPortraitManager::GetIsValidAudioController() for more details.");
	return false;
}

void UPlayerPortraitManager::EnqueuePortraitRequest(
	const ERTSPortraitTypes PortraitType,
	USoundBase* const VoiceLine)
{
	FPortraitQueueEntry NewEntry;
	NewEntry.PortraitType = PortraitType;
	NewEntry.VoiceLine = VoiceLine;

	// Use the actual voice-line duration; fallback to a small non-zero default.
	const float RawDuration = VoiceLine->GetDuration() + 0.8;
	NewEntry.DurationSeconds = FMath::Max(RawDuration, 0.1f);

	if (not NewEntry.IsValid())
	{
		RTSFunctionLibrary::ReportError(
			"UPlayerPortraitManager::EnqueuePortraitRequest created an invalid queue entry.");
		return;
	}

	M_PortraitQueue.Add(MoveTemp(NewEntry));
}

void UPlayerPortraitManager::StartNextPortraitFromQueue()
{
	if (M_PortraitQueue.Num() == 0)
	{
		bM_IsPortraitPlaying = false;
		M_CurrentRequest = FPortraitQueueEntry();

		if (GetIsValidActivePortraitWidget())
		{
			GetActivePortraitWidget()->HidePortrait();
		}
		return;
	}

	if (not GetIsValidActivePortraitWidget() || not GetIsValidAudioController())
	{
		M_PortraitQueue.Reset();
		bM_IsPortraitPlaying = false;
		M_CurrentRequest = FPortraitQueueEntry();
		return;
	}

	FPortraitQueueEntry NextRequest = M_PortraitQueue[0];
	M_PortraitQueue.RemoveAt(0);

	if (not NextRequest.IsValid())
	{
		RTSFunctionLibrary::ReportError(
			"UPlayerPortraitManager::StartNextPortraitFromQueue found an invalid queue entry; skipping.");
		StartNextPortraitFromQueue();
		return;
	}

	bM_IsPortraitPlaying = true;
	M_CurrentRequest = NextRequest;
	UW_Portrait* const PortraitWidget = GetActivePortraitWidget();

	// Show / update the portrait.
	PortraitWidget->UpdatePortrait(NextRequest.PortraitType);

	// Play as custom announcer line; queueing is managed on the portrait manager side.
	M_AudioController->PlayCustomAnnouncerVoiceLine(NextRequest.VoiceLine, true);
	StartSubtitlesForRequest(NextRequest, *PortraitWidget);

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		bM_IsPortraitPlaying = false;
		RTSFunctionLibrary::ReportError(
			"UPlayerPortraitManager::StartNextPortraitFromQueue - World is null, cannot start timer.");
		PortraitWidget->HidePortrait();
		return;
	}

	World->GetTimerManager().ClearTimer(M_CurrentPortraitTimerHandle);
	World->GetTimerManager().SetTimer(
		M_CurrentPortraitTimerHandle,
		this,
		&UPlayerPortraitManager::OnCurrentPortraitFinished,
		NextRequest.DurationSeconds,
		false);
}

void UPlayerPortraitManager::StartSubtitlesForRequest(const FPortraitQueueEntry& Request, UW_Portrait& Widget) const
{
	// Words follow the audio renderer's position in the line: right even if the audio controller queued the line.
	const TWeakObjectPtr<UPlayerAudioController> WeakAudioController = M_AudioController;
	const TWeakObjectPtr<USoundBase> WeakVoiceLine = Request.VoiceLine;
	Widget.StartSubtitles(
		Request.PortraitType,
		Request.VoiceLine,
		[WeakAudioController, WeakVoiceLine]() -> float
		{
			if (not WeakAudioController.IsValid() || not WeakVoiceLine.IsValid())
			{
				return -1.0f;
			}
			return WeakAudioController->GetVoiceLinePlaybackSeconds(WeakVoiceLine.Get());
		});
}

void UPlayerPortraitManager::OnCurrentPortraitFinished()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		bM_IsPortraitPlaying = false;
		return;
	}

	World->GetTimerManager().ClearTimer(M_CurrentPortraitTimerHandle);
	bM_IsPortraitPlaying = false;

	StartNextPortraitFromQueue();
}
