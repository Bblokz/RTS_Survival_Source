// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionLocationIntroSession.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/GameUI/LocationIntro/W_LocationIntro.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace MissionLocationIntroSessionPrivate
{
	// The pan always keeps some time to turn out before it turns back.
	constexpr float MaxReturnShareOfDuration = 0.8f;
	// An intro that cannot get the screen for this long (unpaused) is skipped; it would no longer fit the moment.
	constexpr float MaxSecondsWaitingForScreen = 45.f;

	float EaseInOutSine(const float Alpha)
	{
		return 0.5f - 0.5f * FMath::Cos(PI * FMath::Clamp(Alpha, 0.f, 1.f));
	}

	float GetPhase(const float ElapsedSeconds, const float StartSeconds, const float LengthSeconds)
	{
		if (LengthSeconds <= KINDA_SMALL_NUMBER)
		{
			return ElapsedSeconds >= StartSeconds ? 1.f : 0.f;
		}
		return FMath::Clamp((ElapsedSeconds - StartSeconds) / LengthSeconds, 0.f, 1.f);
	}
}

bool UMissionLocationIntroSession::StartSession(UMissionBase* MissionOwner, const FMissionLocationIntroRequest& Request)
{
	if (not IsValid(MissionOwner))
	{
		RTSFunctionLibrary::ReportError(TEXT("Location intro could not start: the mission is invalid."));
		return false;
	}
	M_Mission = MissionOwner;
	M_Request = Request;
	M_PlayerController = FRTS_Statics::GetRTSController(MissionOwner);
	M_CameraController = FRTS_Statics::GetPlayerCameraController(MissionOwner);
	if (not GetIsValidPlayerController() || not GetIsValidCameraController())
	{
		return false;
	}
	bM_IsWaitingForScreen = true;
	TryTakeScreen();
	return true;
}

void UMissionLocationIntroSession::RequestCancel()
{
	RestoreCameraRotationIfNeeded();
	CompleteSession(ELocationIntroSessionEnd::Cancelled);
}

void UMissionLocationIntroSession::Tick(const float DeltaTime)
{
	if (bM_IsWaitingForScreen)
	{
		TickWaitingForScreen(DeltaTime);
		return;
	}
	if (bM_IsRunning)
	{
		TickRunning();
	}
}

bool UMissionLocationIntroSession::IsTickable() const
{
	return (bM_IsWaitingForScreen || bM_IsRunning) && not HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed);
}

TStatId UMissionLocationIntroSession::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMissionLocationIntroSession, STATGROUP_Tickables);
}

UWorld* UMissionLocationIntroSession::GetTickableGameObjectWorld() const
{
	return M_Mission.IsValid() ? M_Mission->GetWorld() : nullptr;
}

void UMissionLocationIntroSession::TickWaitingForScreen(const float DeltaTime)
{
	if (TryTakeScreen())
	{
		return;
	}
	M_SecondsWaitedForScreen += DeltaTime;
	if (M_SecondsWaitedForScreen < MissionLocationIntroSessionPrivate::MaxSecondsWaitingForScreen)
	{
		return;
	}
	RTSFunctionLibrary::ReportError(TEXT("Location intro skipped: the screen stayed busy (cinematic, takeover, tech "
		"tree or archive)."));
	CompleteSession(ELocationIntroSessionEnd::SkippedScreenBusy);
}

void UMissionLocationIntroSession::TickRunning()
{
	const float ElapsedSeconds = FMath::Min(GetElapsedSeconds(), M_Request.DurationSeconds);
	UpdateCamera(ElapsedSeconds);
	if (GetIsValidWidget(true))
	{
		M_Widget->SetElapsedSeconds(ElapsedSeconds);
	}
	if (ElapsedSeconds >= M_Request.DurationSeconds)
	{
		CompleteSession(ELocationIntroSessionEnd::Finished);
	}
}

bool UMissionLocationIntroSession::TryTakeScreen()
{
	if (not M_Mission.IsValid() || not GetIsValidPlayerController(false))
	{
		return false;
	}
	if (not M_PlayerController->GetCanStartScreenTakeover())
	{
		return false;
	}
	// A MoveCamera move places the pawn every frame until it ends; the intro starts from where it lands.
	if (GetIsValidCameraController() && M_CameraController->GetIsMovingOverTime())
	{
		return false;
	}
	if (not M_Mission->OnCinematicTakeOverFromMission(true))
	{
		return false;
	}
	M_Widget = UW_LocationIntro::ShowIntro(M_PlayerController.Get(), M_Request.Texts, M_Request.Presentation,
	                                       M_Request.DurationSeconds);
	if (not GetIsValidWidget(true))
	{
		M_Mission->OnCinematicTakeOverFromMission(false);
		return false;
	}
	// The pose is taken now, not when the node ran: a cinematic before the intro may have moved the camera.
	CacheStartCameraPose();
	if (M_Request.Presentation.IntroSound != nullptr)
	{
		UGameplayStatics::PlaySound2D(M_Mission.Get(), M_Request.Presentation.IntroSound);
	}
	const UWorld* const World = GetTickableGameObjectWorld();
	M_StartRealSeconds = World != nullptr ? World->GetRealTimeSeconds() : 0.0;
	bM_IsWaitingForScreen = false;
	bM_IsRunning = true;
	return true;
}

void UMissionLocationIntroSession::CacheStartCameraPose()
{
	bM_HasCameraPose = GetIsValidCameraController()
		&& M_CameraController->GetCameraPawnPose(M_StartCameraLocation, M_StartCameraRotation);
	if (not bM_HasCameraPose)
	{
		RTSFunctionLibrary::ReportError(TEXT("Location intro: no camera pawn, the camera will not move."));
	}
}

void UMissionLocationIntroSession::UpdateCamera(const float ElapsedSeconds) const
{
	if (not bM_HasCameraPose || not GetIsValidCameraController(false))
	{
		return;
	}
	using namespace MissionLocationIntroSessionPrivate;
	const float MoveAlpha = EaseInOutSine(GetPhase(ElapsedSeconds, 0.f, M_Request.DurationSeconds));
	const FVector Location = M_StartCameraLocation + M_Request.CameraOffset * MoveAlpha;
	const FRotator Rotation = M_StartCameraRotation + GetPanRotationOffset(ElapsedSeconds);
	M_CameraController->ApplyScriptedCameraPose(Location, Rotation);
}

FRotator UMissionLocationIntroSession::GetPanRotationOffset(const float ElapsedSeconds) const
{
	if (not M_Request.bPanCamera)
	{
		return FRotator::ZeroRotator;
	}
	using namespace MissionLocationIntroSessionPrivate;
	const FLocationIntroPanSettings& Pan = M_Request.PanSettings;
	float PanAmount = 0.f;
	if (Pan.bReturnToStartRotation)
	{
		const float ReturnSeconds = FMath::Min(Pan.ReturnSeconds,
		                                       M_Request.DurationSeconds * MaxReturnShareOfDuration);
		const float TurnOutSeconds = M_Request.DurationSeconds - ReturnSeconds;
		const float TurnOut = EaseInOutSine(GetPhase(ElapsedSeconds, 0.f, TurnOutSeconds));
		const float TurnBack = EaseInOutSine(GetPhase(ElapsedSeconds, TurnOutSeconds, ReturnSeconds));
		PanAmount = TurnOut * (1.f - TurnBack);
	}
	else
	{
		PanAmount = EaseInOutSine(GetPhase(ElapsedSeconds, 0.f, M_Request.DurationSeconds));
	}
	return FRotator(Pan.PitchDegrees * PanAmount, Pan.YawDegrees * PanAmount, 0.f);
}

void UMissionLocationIntroSession::RestoreCameraRotationIfNeeded() const
{
	if (not bM_IsRunning || not M_Request.bPanCamera || not M_Request.PanSettings.bReturnToStartRotation)
	{
		return;
	}
	if (not bM_HasCameraPose || not GetIsValidCameraController(false))
	{
		return;
	}
	FVector CurrentLocation;
	FRotator CurrentRotation;
	if (not M_CameraController->GetCameraPawnPose(CurrentLocation, CurrentRotation))
	{
		return;
	}
	M_CameraController->ApplyScriptedCameraPose(CurrentLocation, M_StartCameraRotation);
}

void UMissionLocationIntroSession::CompleteSession(const ELocationIntroSessionEnd EndReason)
{
	if (bM_HasCompleted)
	{
		return;
	}
	const bool bHeldScreen = bM_IsRunning;
	bM_HasCompleted = true;
	bM_IsRunning = false;
	bM_IsWaitingForScreen = false;

	if (GetIsValidWidget(false))
	{
		M_Widget->CloseIntro();
	}
	M_Widget = nullptr;

	if (not M_Mission.IsValid())
	{
		// The mission is being destroyed: still give the player the HUD and the camera back.
		if (bHeldScreen && GetIsValidPlayerController(false))
		{
			M_PlayerController->OnCinematicTakeOver(false);
		}
		return;
	}
	if (bHeldScreen)
	{
		M_Mission->OnCinematicTakeOverFromMission(false);
	}
	M_Mission->OnLocationIntroSessionEnded(this, M_Request.ID, EndReason);
}

float UMissionLocationIntroSession::GetElapsedSeconds() const
{
	const UWorld* const World = GetTickableGameObjectWorld();
	if (World == nullptr)
	{
		return 0.f;
	}
	return static_cast<float>(World->GetRealTimeSeconds() - M_StartRealSeconds);
}

bool UMissionLocationIntroSession::GetIsValidPlayerController(const bool bReportError) const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	if (bReportError)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this, TEXT("M_PlayerController"), TEXT("UMissionLocationIntroSession::GetIsValidPlayerController"), this);
	}
	return false;
}

bool UMissionLocationIntroSession::GetIsValidCameraController(const bool bReportError) const
{
	if (M_CameraController.IsValid())
	{
		return true;
	}
	if (bReportError)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this, TEXT("M_CameraController"), TEXT("UMissionLocationIntroSession::GetIsValidCameraController"), this);
	}
	return false;
}

bool UMissionLocationIntroSession::GetIsValidWidget(const bool bReportError) const
{
	if (IsValid(M_Widget))
	{
		return true;
	}
	if (bReportError)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this, TEXT("M_Widget"), TEXT("UMissionLocationIntroSession::GetIsValidWidget"), this);
	}
	return false;
}
