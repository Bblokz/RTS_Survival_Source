// Copyright (C) Bas Blokzijl - All rights reserved.

// The location intro part of UMissionBase: a cinematic "place and date" card over a drifting player camera.

#include "MissionBase.h"

#include "MissionLocationIntroSession.h"
#include "RTS_Survival/GameUI/LocationIntro/W_LocationIntro.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace MissionLocationIntroPrivate
{
	// Used when the duration pin is left at zero.
	constexpr float DefaultDurationSeconds = 7.f;
}

bool UMissionBase::ShowLocationIntro(const FText& Location,
                                     const FText& Date,
                                     const ELocationIntroDriftAxis DriftAxis,
                                     const float DriftDistance,
                                     const float DurationSeconds,
                                     const bool bPanCamera,
                                     const FLocationIntroPanSettings& PanSettings,
                                     const int32 ID,
                                     const FText& LocationSubtitle,
                                     const FText& TimeOfDay,
                                     const FLocationIntroPresentation& Presentation)
{
	if (M_ActiveLocationIntroSession != nullptr)
	{
		RTSFunctionLibrary::ReportError("ShowLocationIntro: mission " + GetName()
			+ " already runs a location intro; this one is skipped.");
		LocationIntro_ReportSkippedNextFrame(ID);
		return false;
	}

	FMissionLocationIntroRequest Request;
	Request.Texts.Location = Location;
	Request.Texts.LocationSubtitle = LocationSubtitle;
	Request.Texts.Date = Date;
	Request.Texts.TimeOfDay = TimeOfDay;
	Request.Presentation = Presentation;
	Request.CameraOffset = DriftAxis == ELocationIntroDriftAxis::WorldX
		                       ? FVector(DriftDistance, 0.f, 0.f)
		                       : FVector(0.f, DriftDistance, 0.f);
	Request.bPanCamera = bPanCamera;
	Request.PanSettings = PanSettings;
	using namespace MissionLocationIntroPrivate;
	const float RequestedSeconds = DurationSeconds > 0.f ? DurationSeconds : DefaultDurationSeconds;
	Request.DurationSeconds = FMath::Max(RequestedSeconds,
	                                     UW_LocationIntro::GetMinDurationSeconds(Request.Texts, Presentation));
	Request.ID = ID;

	UMissionLocationIntroSession* const Session = NewObject<UMissionLocationIntroSession>(this);
	M_ActiveLocationIntroSession = Session;
	if (not Session->StartSession(this, Request))
	{
		M_ActiveLocationIntroSession = nullptr;
		LocationIntro_ReportSkippedNextFrame(ID);
		return false;
	}
	return true;
}

bool UMissionBase::GetIsLocationIntroPlaying() const
{
	return M_ActiveLocationIntroSession != nullptr && not M_ActiveLocationIntroSession->GetHasCompleted();
}

void UMissionBase::CancelActiveLocationIntro()
{
	if (M_ActiveLocationIntroSession == nullptr)
	{
		return;
	}
	// The session clears M_ActiveLocationIntroSession through OnLocationIntroSessionEnded.
	M_ActiveLocationIntroSession->RequestCancel();
	M_ActiveLocationIntroSession = nullptr;
}

void UMissionBase::OnLocationIntroSessionEnded(const UMissionLocationIntroSession* Session, const int32 ID,
                                               const ELocationIntroSessionEnd EndReason)
{
	if (M_ActiveLocationIntroSession != Session)
	{
		return;
	}
	M_ActiveLocationIntroSession = nullptr;
	switch (EndReason)
	{
	case ELocationIntroSessionEnd::Finished:
		BP_OnLocationIntroFinished(ID, true);
		return;
	case ELocationIntroSessionEnd::SkippedScreenBusy:
		BP_OnLocationIntroFinished(ID, false);
		return;
	case ELocationIntroSessionEnd::Cancelled:
		return;
	}
}

void UMissionBase::LocationIntro_ReportSkippedNextFrame(const int32 ID)
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	// Next frame, so a Blueprint that chains on the event never re-enters the node that called it.
	TWeakObjectPtr<UMissionBase> WeakMission(this);
	World->GetTimerManager().SetTimerForNextTick([WeakMission, ID]()
	{
		if (not WeakMission.IsValid())
		{
			return;
		}
		WeakMission->BP_OnLocationIntroFinished(ID, false);
	});
}
