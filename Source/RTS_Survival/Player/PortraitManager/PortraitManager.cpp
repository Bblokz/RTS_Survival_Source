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
	if (M_PortraitWidget.IsValid())
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

		if (GetIsValidPortraitWidget())
		{
			M_PortraitWidget->HidePortrait();
		}
		return;
	}

	if (not GetIsValidPortraitWidget() || not GetIsValidAudioController())
	{
		M_PortraitQueue.Reset();
		bM_IsPortraitPlaying = false;
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

	// Show / update the portrait.
	M_PortraitWidget->UpdatePortrait(NextRequest.PortraitType);

	// Play as custom announcer line; queueing is managed on the portrait manager side.
	M_AudioController->PlayCustomAnnouncerVoiceLine(NextRequest.VoiceLine, true);

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(
			"UPlayerPortraitManager::StartNextPortraitFromQueue - World is null, cannot start timer.");
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

void UPlayerPortraitManager::OnCurrentPortraitFinished()
{
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_CurrentPortraitTimerHandle);
	}

	bM_IsPortraitPlaying = false;

	StartNextPortraitFromQueue();
}
