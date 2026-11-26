// Copyright (C) Bas Blokzijl - All rights reserved.

// The camera tour and fog reveal part of UMissionBase: a short in-map cinematic over the RTS camera.

#include "MissionBase.h"

#include "MissionCameraTourSession.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ReconFlight/GlobalAbilityVisionBubble.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace MissionCameraTourPrivate
{
	// The player's index for fog of war vision (AGlobalAbilityVisionBubble lights the fog only for player 1).
	constexpr uint8 PlayerFowIndex = 1;
}

bool UMissionBase::PlayCameraTour(const TArray<FMissionCameraTourShot>& Shots,
                                  const FMissionCameraTourSettings& Settings, const int32 ID)
{
	if (M_ActiveCameraTourSession != nullptr)
	{
		RTSFunctionLibrary::ReportError("PlayCameraTour: mission " + GetName()
			+ " already runs a camera tour; this one is skipped.");
		CameraTour_ReportFinishedNextFrame(ID, false);
		return false;
	}
	UMissionCameraTourSession* const Session = NewObject<UMissionCameraTourSession>(this);
	M_ActiveCameraTourSession = Session;
	if (not Session->StartSession(this, Shots, Settings, ID))
	{
		M_ActiveCameraTourSession = nullptr;
		CameraTour_ReportFinishedNextFrame(ID, false);
		return false;
	}
	return true;
}

bool UMissionBase::GetIsCameraTourPlaying() const
{
	return M_ActiveCameraTourSession != nullptr && not M_ActiveCameraTourSession->GetHasCompleted();
}

int32 UMissionBase::GetCameraTourShotIndex() const
{
	if (not GetIsCameraTourPlaying())
	{
		return INDEX_NONE;
	}
	return M_ActiveCameraTourSession->GetCurrentShotIndex();
}

AActor* UMissionBase::RevealAreaForSeconds(const FVector Location, const float Radius, const float Seconds)
{
	UWorld* const World = GetWorld();
	if (World == nullptr || Radius <= 0.f || Seconds <= 0.f)
	{
		RTSFunctionLibrary::ReportError("RevealAreaForSeconds: no world, or radius / seconds not above zero. Mission: "
			+ GetName());
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGlobalAbilityVisionBubble* const Bubble = World->SpawnActor<AGlobalAbilityVisionBubble>(
		AGlobalAbilityVisionBubble::StaticClass(), Location, FRotator::ZeroRotator, SpawnParameters);
	if (not IsValid(Bubble))
	{
		RTSFunctionLibrary::ReportError("RevealAreaForSeconds could not spawn the vision bubble. Mission: " + GetName());
		return nullptr;
	}
	Bubble->StartVision(Radius, Seconds, MissionCameraTourPrivate::PlayerFowIndex);
	return Bubble;
}

void UMissionBase::CancelActiveCameraTour()
{
	if (M_ActiveCameraTourSession == nullptr)
	{
		return;
	}
	UMissionCameraTourSession* const Session = M_ActiveCameraTourSession;
	// Cleared first: the session reports Cancelled through OnCameraTourSessionEnded, which then has nothing to do.
	M_ActiveCameraTourSession = nullptr;
	Session->RequestCancel();
}

void UMissionBase::OnCameraTourSessionEnded(const UMissionCameraTourSession* Session, const int32 ID,
                                            const EMissionCameraTourEnd EndReason)
{
	if (M_ActiveCameraTourSession != Session)
	{
		return;
	}
	M_ActiveCameraTourSession = nullptr;
	switch (EndReason)
	{
	case EMissionCameraTourEnd::Finished:
		OnCameraTourFinishedNative(ID, true);
		BP_OnCameraTourFinished(ID, true);
		return;
	case EMissionCameraTourEnd::SkippedScreenBusy:
		OnCameraTourFinishedNative(ID, false);
		BP_OnCameraTourFinished(ID, false);
		return;
	case EMissionCameraTourEnd::Cancelled:
		return;
	}
}

void UMissionBase::OnCameraTourShotStarted(const UMissionCameraTourSession* Session, const int32 ID,
                                            const int32 ShotIndex)
{
	if (M_ActiveCameraTourSession != Session)
	{
		return;
	}
	OnCameraTourShotStartedNative(ID, ShotIndex);
	BP_OnCameraTourShotStarted(ID, ShotIndex);
}

void UMissionBase::CameraTour_ReportFinishedNextFrame(const int32 ID, const bool bWasShown)
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	// Next frame, so a Blueprint that chains on the event never re-enters the node that called it.
	TWeakObjectPtr<UMissionBase> WeakMission(this);
	World->GetTimerManager().SetTimerForNextTick([WeakMission, ID, bWasShown]()
	{
		if (not WeakMission.IsValid())
		{
			return;
		}
		WeakMission->OnCameraTourFinishedNative(ID, bWasShown);
		WeakMission->BP_OnCameraTourFinished(ID, bWasShown);
	});
}
