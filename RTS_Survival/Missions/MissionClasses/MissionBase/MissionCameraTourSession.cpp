// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionCameraTourSession.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/GameUI/CinematicOverlay/W_CinematicOverlay.h"
#include "RTS_Survival/GameUI/CameraTour/W_MissionCameraTour.h"
#include "RTS_Survival/GameUI/HoldToConfirm/W_HoldToSkip.h"
#include "RTS_Survival/GameUI/LocationIntro/W_LocationIntro.h"
#include "RTS_Survival/GameUI/Portrait/PortraitWidget/W_Portrait.h"
#include "RTS_Survival/Player/PortraitManager/PortraitManager.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace MissionCameraTourSessionPrivate
{
	// A tour that cannot get the screen for this long (unpaused) is skipped; it would no longer fit the moment.
	constexpr float MaxSecondsWaitingForScreen = 45.f;
	constexpr float BarsInSeconds = 0.8f;
	constexpr float BarsOutMaxSeconds = 0.8f;
	constexpr float CaptionFadeInDelaySeconds = 0.35f;
	constexpr float CaptionFadeInSeconds = 0.7f;
	constexpr float CaptionFadeOutSeconds = 0.6f;
	// Below this downward slope the camera looks almost level and the ground-point correction would explode.
	constexpr float MinDownwardForwardZ = 0.1f;
	// The fog stays lifted this long after the tour, so the player still sees the place as the view returns.
	constexpr float RevealMarginSeconds = 4.f;
	// Above the tour widget (W_MissionCameraTour, 6) and the voice-line subtitle strip (7): the prompt sits at the
	// bottom of the screen, where the tour's cinema bar would otherwise cover it.
	constexpr int32 SkipWidgetZOrder = 8;
	// Opening title length when its DurationSeconds is left at 0.
	constexpr float DefaultTitleSeconds = 7.f;
	// A shot's voice line ends at least this long before the shot does.
	constexpr float VoiceLineTailSeconds = 0.4f;

	float EaseInOutSine(const float Alpha)
	{
		return 0.5f - 0.5f * FMath::Cos(PI * FMath::Clamp(Alpha, 0.f, 1.f));
	}

	float EaseOutCubic(const float Alpha)
	{
		return 1.f - FMath::Pow(1.f - FMath::Clamp(Alpha, 0.f, 1.f), 3.f);
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

bool UMissionCameraTourSession::StartSession(UMissionBase* MissionOwner, const TArray<FMissionCameraTourShot>& Shots,
                                             const FMissionCameraTourSettings& Settings, const int32 ID)
{
	if (not IsValid(MissionOwner))
	{
		RTSFunctionLibrary::ReportError(TEXT("Camera tour could not start: the mission is invalid."));
		return false;
	}
	if (Shots.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Camera tour of mission " + MissionOwner->GetName() + " has no shots.");
		return false;
	}
	M_Mission = MissionOwner;
	M_Shots = Shots;
	for (FMissionCameraTourShot& Shot : M_Shots)
	{
		Shot.Seconds = FMath::Max(0.5f, Shot.Seconds);
		Shot.BlendInSeconds = FMath::Clamp(Shot.BlendInSeconds, 0.f, Shot.Seconds);
	}
	M_Settings = Settings;
	M_Settings.ReturnSeconds = FMath::Max(0.f, M_Settings.ReturnSeconds);
	FitShotsToVoiceLines();
	M_ID = ID;
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

void UMissionCameraTourSession::RequestCancel()
{
	CompleteSession(EMissionCameraTourEnd::Cancelled);
}

void UMissionCameraTourSession::Tick(const float DeltaTime)
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

bool UMissionCameraTourSession::IsTickable() const
{
	return (bM_IsWaitingForScreen || bM_IsRunning) && not HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed);
}

TStatId UMissionCameraTourSession::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMissionCameraTourSession, STATGROUP_Tickables);
}

UWorld* UMissionCameraTourSession::GetTickableGameObjectWorld() const
{
	return M_Mission.IsValid() ? M_Mission->GetWorld() : nullptr;
}

void UMissionCameraTourSession::TickWaitingForScreen(const float DeltaTime)
{
	if (TryTakeScreen())
	{
		return;
	}
	M_SecondsWaitedForScreen += DeltaTime;
	if (M_SecondsWaitedForScreen < MissionCameraTourSessionPrivate::MaxSecondsWaitingForScreen)
	{
		return;
	}
	RTSFunctionLibrary::ReportError(TEXT("Camera tour skipped: the screen stayed busy (cinematic, takeover, tech "
		"tree or archive)."));
	CompleteSession(EMissionCameraTourEnd::SkippedScreenBusy);
}

void UMissionCameraTourSession::TickRunning()
{
	using namespace MissionCameraTourSessionPrivate;
	const float ElapsedSeconds = FMath::Min(GetElapsedSeconds(), GetTotalSeconds());
	const float ShotsTotalSeconds = GetShotsTotalSeconds();

	if (ElapsedSeconds < ShotsTotalSeconds)
	{
		int32 ShotIndex = M_Shots.Num() - 1;
		for (int32 Index = 0; Index < M_Shots.Num(); ++Index)
		{
			if (ElapsedSeconds < GetShotStartSeconds(Index + 1))
			{
				ShotIndex = Index;
				break;
			}
		}
		if (ShotIndex != M_CurrentShotIndex)
		{
			EnterShot(ShotIndex);
			// The mission hears of the shot in Blueprint, which may end the tour there and then.
			if (not bM_IsRunning)
			{
				return;
			}
		}
		const FMissionCameraTourShot& Shot = M_Shots[ShotIndex];
		const float ShotElapsed = ElapsedSeconds - GetShotStartSeconds(ShotIndex);
		TryPlayShotVoiceLine(ShotIndex, ShotElapsed);
		const FTourPose ShotPose = GetShotPose(Shot, EaseInOutSine(ShotElapsed / Shot.Seconds));
		const float BlendAlpha = EaseInOutSine(GetPhase(ShotElapsed, 0.f, Shot.BlendInSeconds));
		ApplyPose(BlendPoses(M_BlendFromPose, ShotPose, BlendAlpha));
	}
	else
	{
		if (not bM_IsReturning)
		{
			EnterReturn();
		}
		const float ReturnAlpha = EaseInOutSine(GetPhase(ElapsedSeconds, ShotsTotalSeconds, M_Settings.ReturnSeconds));
		ApplyPose(BlendPoses(M_BlendFromPose, M_PlayerPose, ReturnAlpha));
	}
	UpdateWidget(ElapsedSeconds);
	UpdateOpeningTitle(ElapsedSeconds);

	if (ElapsedSeconds >= GetTotalSeconds())
	{
		CompleteSession(EMissionCameraTourEnd::Finished);
	}
}

UPlayerPortraitManager* UMissionCameraTourSession::GetPortraitManager() const
{
	return M_PlayerController.IsValid() ? M_PlayerController->GetPlayerPortraitManager() : nullptr;
}

UMissionCameraTourSession::FHudPortraitPlace UMissionCameraTourSession::GetHudPortraitPlace() const
{
	FHudPortraitPlace Place;
	const UPlayerPortraitManager* const PortraitManager = GetPortraitManager();
	UW_Portrait* const HudPortrait = IsValid(PortraitManager) ? PortraitManager->GetHudPortraitWidget() : nullptr;
	if (not IsValid(HudPortrait))
	{
		return Place;
	}
	Place.PortraitClass = HudPortrait->GetClass();
	// The portrait is laid out while hidden between lines (Hidden, not Collapsed), so its geometry is current.
	const FGeometry& Geometry = HudPortrait->GetCachedGeometry();
	const FVector2D LocalSize(Geometry.GetLocalSize());
	if (LocalSize.X < 1.0 || LocalSize.Y < 1.0)
	{
		return Place;
	}
	FVector2D PixelTopLeft;
	FVector2D ViewportTopLeft;
	FVector2D PixelBottomRight;
	FVector2D ViewportBottomRight;
	USlateBlueprintLibrary::AbsoluteToViewport(M_Mission.Get(), FVector2D(Geometry.GetAbsolutePosition()),
	                                           PixelTopLeft, ViewportTopLeft);
	USlateBlueprintLibrary::AbsoluteToViewport(M_Mission.Get(), FVector2D(Geometry.LocalToAbsolute(LocalSize)),
	                                           PixelBottomRight, ViewportBottomRight);
	const FVector2D ViewportSize = ViewportBottomRight - ViewportTopLeft;
	if (ViewportSize.X < 1.0 || ViewportSize.Y < 1.0)
	{
		return Place;
	}
	Place.ViewportPosition = ViewportTopLeft;
	Place.ViewportSize = ViewportSize;
	Place.bHasRect = true;
	return Place;
}

void UMissionCameraTourSession::HostPortraitOnTour(const FHudPortraitPlace& HudPortraitPlace)
{
	if (not GetIsValidWidget(false) || HudPortraitPlace.PortraitClass == nullptr)
	{
		return;
	}
	UPlayerPortraitManager* const PortraitManager = GetPortraitManager();
	if (not IsValid(PortraitManager))
	{
		return;
	}
	UW_Portrait* const PortraitCopy = M_Widget->AddPortraitCopy(HudPortraitPlace.PortraitClass,
	                                                            HudPortraitPlace.ViewportPosition,
	                                                            HudPortraitPlace.ViewportSize,
	                                                            not HudPortraitPlace.bHasRect);
	if (PortraitCopy == nullptr)
	{
		return;
	}
	PortraitManager->SetCinematicPortraitHost(PortraitCopy);
}

void UMissionCameraTourSession::ReleaseTourPortrait() const
{
	if (not GetIsValidWidget(false) || M_Widget->GetPortraitCopy() == nullptr)
	{
		return;
	}
	if (UPlayerPortraitManager* const PortraitManager = GetPortraitManager())
	{
		PortraitManager->ClearCinematicPortraitHost(M_Widget->GetPortraitCopy());
	}
}

bool UMissionCameraTourSession::TryTakeScreen()
{
	if (not M_Mission.IsValid() || not GetIsValidPlayerController(false))
	{
		return false;
	}
	if (not M_PlayerController->GetCanStartScreenTakeover())
	{
		return false;
	}
	// The start-game screen pauses the game without the lock; a cinematic started behind it would play unseen on its
	// real-time clock. Waiting here costs no wait time: the paused game does not tick this session.
	const UPlayerStartGameControl* const StartGameControl = M_PlayerController->FindComponentByClass<
		UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		return false;
	}
	// A MoveCamera move places the pawn every frame until it ends; the tour starts after it.
	if (GetIsValidCameraController() && M_CameraController->GetIsMovingOverTime())
	{
		return false;
	}
	if (not CachePlayerPose())
	{
		return false;
	}
	// Read where the HUD portrait sits while the HUD is still laid out; the takeover hides it.
	const FHudPortraitPlace HudPortraitPlace = GetHudPortraitPlace();
	if (not M_Mission->OnCinematicTakeOverFromMission(true))
	{
		return false;
	}
	M_Widget = UW_MissionCameraTour::ShowTour(M_PlayerController.Get(), M_Settings.bShowLetterbox,
	                                          M_Settings.LetterboxHeightFraction);
	HostPortraitOnTour(HudPortraitPlace);
	ShowOpeningTitle();
	PlayStartSound();
	const UWorld* const World = GetTickableGameObjectWorld();
	M_StartRealSeconds = World != nullptr ? World->GetRealTimeSeconds() : 0.0;
	M_BlendFromPose = M_PlayerPose;
	M_LastAppliedPose = M_PlayerPose;
	bM_IsWaitingForScreen = false;
	bM_IsRunning = true;
	RevealShotAreas();
	if (M_Settings.bAllowSkipping)
	{
		CreateSkipWidget();
	}
	return true;
}

void UMissionCameraTourSession::FitShotsToVoiceLines()
{
	for (FMissionCameraTourShot& Shot : M_Shots)
	{
		if (Shot.VoiceLine == nullptr)
		{
			continue;
		}
		Shot.VoiceLineDelaySeconds = FMath::Max(0.f, Shot.VoiceLineDelaySeconds);
		const float LineSeconds = Shot.VoiceLine->GetDuration();
		// Looping or procedural sounds report a huge duration; those keep the authored length.
		if (LineSeconds <= 0.f || LineSeconds >= INDEFINITELY_LOOPING_DURATION)
		{
			continue;
		}
		const float NeededSeconds = Shot.VoiceLineDelaySeconds + LineSeconds
			+ MissionCameraTourSessionPrivate::VoiceLineTailSeconds;
		Shot.Seconds = FMath::Max(Shot.Seconds, NeededSeconds);
	}
}

void UMissionCameraTourSession::ShowOpeningTitle()
{
	const FMissionCameraTourOpeningTitle& Title = M_Settings.OpeningTitle;
	if (not Title.bShowTitle || not GetIsValidPlayerController(false))
	{
		return;
	}
	FLocationIntroPresentation Presentation = Title.Presentation;
	// One set of bars: the title's sit exactly under the tour's, so its own fade-out never shows.
	Presentation.bShowLetterbox = M_Settings.bShowLetterbox;
	Presentation.LetterboxHeightFraction = M_Settings.LetterboxHeightFraction;
	const float RequestedSeconds = Title.DurationSeconds > 0.f
		                               ? Title.DurationSeconds
		                               : MissionCameraTourSessionPrivate::DefaultTitleSeconds;
	M_TitleSeconds = FMath::Max(RequestedSeconds, UW_LocationIntro::GetMinDurationSeconds(Title.Texts, Presentation));
	M_TitleWidget = UW_LocationIntro::ShowIntro(M_PlayerController.Get(), Title.Texts, Presentation, M_TitleSeconds);
	if (M_TitleWidget == nullptr)
	{
		M_TitleSeconds = 0.f;
		return;
	}
	if (Presentation.IntroSound != nullptr && M_Mission.IsValid())
	{
		UGameplayStatics::PlaySound2D(M_Mission.Get(), Presentation.IntroSound);
	}
}

void UMissionCameraTourSession::UpdateOpeningTitle(const float ElapsedSeconds)
{
	if (not IsValid(M_TitleWidget))
	{
		return;
	}
	if (bM_IsReturning || ElapsedSeconds >= M_TitleSeconds)
	{
		CloseOpeningTitle();
		return;
	}
	M_TitleWidget->SetElapsedSeconds(ElapsedSeconds);
}

void UMissionCameraTourSession::CloseOpeningTitle()
{
	if (IsValid(M_TitleWidget))
	{
		M_TitleWidget->CloseIntro();
	}
	M_TitleWidget = nullptr;
}

void UMissionCameraTourSession::TryPlayShotVoiceLine(const int32 ShotIndex, const float ShotElapsedSeconds)
{
	if (ShotIndex <= M_LastVoicedShotIndex || not M_Shots.IsValidIndex(ShotIndex))
	{
		return;
	}
	const FMissionCameraTourShot& Shot = M_Shots[ShotIndex];
	if (Shot.VoiceLine == nullptr)
	{
		M_LastVoicedShotIndex = ShotIndex;
		return;
	}
	if (ShotElapsedSeconds < Shot.VoiceLineDelaySeconds || not M_Mission.IsValid())
	{
		return;
	}
	M_LastVoicedShotIndex = ShotIndex;
	M_Mission->PlayPortrait(Shot.VoiceLinePortrait, Shot.VoiceLine);
}

void UMissionCameraTourSession::PlayStartSound() const
{
	if (M_Settings.StartSound == nullptr || not M_Mission.IsValid())
	{
		return;
	}
	// On its own audio component, so the subtitle strip can follow the playback; the strip keeps the component alive
	// and stays until the line ends, also when the tour is skipped first.
	UAudioComponent* const VoiceLine = UGameplayStatics::CreateSound2D(M_Mission.Get(), M_Settings.StartSound);
	const UW_CinematicOverlay* const Overlay = IsValid(VoiceLine) && GetIsValidPlayerController(false)
		                                           ? UW_CinematicOverlay::ShowForVoiceLine(
			                                           M_PlayerController.Get(), VoiceLine,
			                                           M_PlayerController->M_CinematicOverlayWidgetClass)
		                                           : nullptr;
	if (Overlay == nullptr)
	{
		// No transcript: nothing holds the component, so play it the fire-and-forget way.
		if (IsValid(VoiceLine))
		{
			VoiceLine->DestroyComponent();
		}
		UGameplayStatics::PlaySound2D(M_Mission.Get(), M_Settings.StartSound);
		return;
	}
	VoiceLine->Play();
}

void UMissionCameraTourSession::RevealShotAreas() const
{
	if (not M_Mission.IsValid())
	{
		return;
	}
	const float RevealSeconds = GetTotalSeconds() + MissionCameraTourSessionPrivate::RevealMarginSeconds;
	for (int32 ShotIndex = 0; ShotIndex < M_Shots.Num(); ++ShotIndex)
	{
		const FMissionCameraTourShot& Shot = M_Shots[ShotIndex];
		if (Shot.RevealRadius <= 0.f)
		{
			continue;
		}
		// The blend in from the previous shot crosses ground too.
		if (ShotIndex > 0)
		{
			RevealAlongPath(M_Shots[ShotIndex - 1].FocusEnd, Shot.FocusStart, Shot.RevealRadius, RevealSeconds);
		}
		RevealAlongPath(Shot.FocusStart, Shot.FocusEnd, Shot.RevealRadius, RevealSeconds);
	}
}

void UMissionCameraTourSession::RevealAlongPath(const FVector& From, const FVector& To, const float Radius,
                                                const float Seconds) const
{
	// Bubbles one radius apart overlap, so the whole strip the camera passes over is lifted.
	const float PathLength = FVector::Dist2D(From, To);
	const int32 StepCount = FMath::Max(1, FMath::CeilToInt(PathLength / Radius));
	for (int32 Step = 0; Step <= StepCount; ++Step)
	{
		M_Mission->RevealAreaForSeconds(FMath::Lerp(From, To, static_cast<float>(Step) / StepCount), Radius, Seconds);
	}
}

void UMissionCameraTourSession::CreateSkipWidget()
{
	if (not GetIsValidPlayerController() || not M_PlayerController->GetIsValidHoldToSkipWidgetClass())
	{
		return;
	}
	M_SkipWidget = CreateWidget<UW_HoldToSkip>(M_PlayerController.Get(), M_PlayerController->M_HoldToSkipWidgetClass);
	if (not GetIsValidSkipWidget())
	{
		return;
	}
	// The prompt hears Space through its own input preprocessor, so the game keeps its input mode.
	M_SkipWidget->AddToViewport(MissionCameraTourSessionPrivate::SkipWidgetZOrder);
	M_SkipWidget->OnHoldCompleted.AddDynamic(this, &UMissionCameraTourSession::HandleSkipHoldCompleted);
}

void UMissionCameraTourSession::RemoveSkipWidget()
{
	// Only skippable tours ever have the prompt, so a missing one is no error here.
	if (not GetIsValidSkipWidget(false))
	{
		M_SkipWidget = nullptr;
		return;
	}
	M_SkipWidget->OnHoldCompleted.RemoveDynamic(this, &UMissionCameraTourSession::HandleSkipHoldCompleted);
	M_SkipWidget->StopHold();
	M_SkipWidget->RemoveFromParent();
	M_SkipWidget = nullptr;
}

void UMissionCameraTourSession::HandleSkipHoldCompleted()
{
	RemoveSkipWidget();
	CloseOpeningTitle();
	const UWorld* const World = GetTickableGameObjectWorld();
	if (not bM_IsRunning || bM_IsReturning || World == nullptr)
	{
		return;
	}
	M_StartRealSeconds = World->GetRealTimeSeconds() - GetShotsTotalSeconds();
}

bool UMissionCameraTourSession::CachePlayerPose()
{
	if (not GetIsValidCameraController())
	{
		return false;
	}
	FRotator PawnRotation;
	float ArmLength = 0.f;
	if (not M_CameraController->GetCameraPawnPose(M_PlayerPawnLocation, PawnRotation)
		|| not M_CameraController->GetScriptedCameraArmState(ArmLength, M_ArmRelativeRotation))
	{
		RTSFunctionLibrary::ReportError(TEXT("Camera tour: no camera pawn or spring arm, the tour cannot show."));
		bM_HasCameraPose = false;
		return false;
	}
	// The focus at the pawn's own height maps back onto the pawn exactly (see ApplyPose).
	M_PlayerPose.Focus = M_PlayerPawnLocation;
	M_PlayerPose.Yaw = PawnRotation.Yaw;
	M_PlayerPose.Pitch = PawnRotation.Pitch;
	M_PlayerPose.ArmLength = ArmLength;
	bM_HasCameraPose = true;
	return true;
}

float UMissionCameraTourSession::GetShotStartSeconds(const int32 ShotIndex) const
{
	float StartSeconds = 0.f;
	for (int32 Index = 0; Index < ShotIndex && Index < M_Shots.Num(); ++Index)
	{
		StartSeconds += M_Shots[Index].Seconds;
	}
	return StartSeconds;
}

float UMissionCameraTourSession::GetShotsTotalSeconds() const
{
	return GetShotStartSeconds(M_Shots.Num());
}

float UMissionCameraTourSession::GetTotalSeconds() const
{
	return GetShotsTotalSeconds() + M_Settings.ReturnSeconds;
}

void UMissionCameraTourSession::EnterShot(const int32 ShotIndex)
{
	M_CurrentShotIndex = ShotIndex;
	M_BlendFromPose = M_LastAppliedPose;
	if (GetIsValidWidget(false) && M_Shots.IsValidIndex(ShotIndex))
	{
		M_Widget->SetCaption(M_Shots[ShotIndex].Title, M_Shots[ShotIndex].Subtitle);
	}
	if (M_Mission.IsValid())
	{
		M_Mission->OnCameraTourShotStarted(this, M_ID, ShotIndex);
	}
}

void UMissionCameraTourSession::EnterReturn()
{
	bM_IsReturning = true;
	M_CurrentShotIndex = INDEX_NONE;
	M_BlendFromPose = M_LastAppliedPose;
}

UMissionCameraTourSession::FTourPose UMissionCameraTourSession::GetShotPose(const FMissionCameraTourShot& Shot,
                                                                           const float ShotAlpha) const
{
	FTourPose Pose;
	Pose.Focus = FMath::Lerp(Shot.FocusStart, Shot.FocusEnd, ShotAlpha);
	Pose.Yaw = Shot.YawStart + FMath::FindDeltaAngleDegrees(Shot.YawStart, Shot.YawEnd) * ShotAlpha;
	Pose.Pitch = FMath::Lerp(Shot.PitchStart, Shot.PitchEnd, ShotAlpha);
	const float ArmStart = Shot.ArmLengthStart > 0.f ? Shot.ArmLengthStart : M_PlayerPose.ArmLength;
	const float ArmEnd = Shot.ArmLengthEnd > 0.f ? Shot.ArmLengthEnd : M_PlayerPose.ArmLength;
	Pose.ArmLength = FMath::Lerp(ArmStart, ArmEnd, ShotAlpha);
	return Pose;
}

UMissionCameraTourSession::FTourPose UMissionCameraTourSession::BlendPoses(const FTourPose& From,
                                                                          const FTourPose& To, const float Alpha)
{
	FTourPose Pose;
	Pose.Focus = FMath::Lerp(From.Focus, To.Focus, Alpha);
	Pose.Yaw = From.Yaw + FMath::FindDeltaAngleDegrees(From.Yaw, To.Yaw) * Alpha;
	Pose.Pitch = FMath::Lerp(From.Pitch, To.Pitch, Alpha);
	Pose.ArmLength = FMath::Lerp(From.ArmLength, To.ArmLength, Alpha);
	return Pose;
}

void UMissionCameraTourSession::ApplyPose(const FTourPose& Pose)
{
	M_LastAppliedPose = Pose;
	if (not bM_HasCameraPose || not GetIsValidCameraController(false))
	{
		return;
	}
	const FRotator PawnRotation(Pose.Pitch, Pose.Yaw, 0.f);
	// The camera hangs on the spring arm behind the pawn and looks through it. The pawn keeps its own height, so to
	// put a ground point at another height in the middle of the screen the pawn slides along the view direction.
	const FVector Forward = (PawnRotation.Quaternion() * M_ArmRelativeRotation.Quaternion()).GetForwardVector();
	FVector PawnLocation(Pose.Focus.X, Pose.Focus.Y, M_PlayerPawnLocation.Z);
	if (Forward.Z < -MissionCameraTourSessionPrivate::MinDownwardForwardZ)
	{
		const float HeightAbovePawn = Pose.Focus.Z - M_PlayerPawnLocation.Z;
		PawnLocation.X -= Forward.X * HeightAbovePawn / Forward.Z;
		PawnLocation.Y -= Forward.Y * HeightAbovePawn / Forward.Z;
	}
	M_CameraController->ApplyScriptedCameraPose(PawnLocation, PawnRotation);
	M_CameraController->ApplyScriptedCameraArmLength(Pose.ArmLength);
}

void UMissionCameraTourSession::UpdateWidget(const float ElapsedSeconds) const
{
	if (not GetIsValidWidget(false))
	{
		return;
	}
	using namespace MissionCameraTourSessionPrivate;
	const float TotalSeconds = GetTotalSeconds();
	const float BarsOutSeconds = FMath::Min(BarsOutMaxSeconds, M_Settings.ReturnSeconds);
	const float BarsIn = EaseOutCubic(GetPhase(ElapsedSeconds, 0.f, BarsInSeconds));
	const float BarsOut = EaseInOutSine(GetPhase(ElapsedSeconds, TotalSeconds - BarsOutSeconds, BarsOutSeconds));
	M_Widget->SetBars(BarsIn * (1.f - BarsOut));

	if (not M_Shots.IsValidIndex(M_CurrentShotIndex))
	{
		M_Widget->SetCaptionVisibility(0.f);
		return;
	}
	const float ShotSeconds = M_Shots[M_CurrentShotIndex].Seconds;
	const float ShotElapsed = ElapsedSeconds - GetShotStartSeconds(M_CurrentShotIndex);
	const float FadeIn = EaseInOutSine(GetPhase(ShotElapsed, CaptionFadeInDelaySeconds, CaptionFadeInSeconds));
	const float FadeOut = EaseInOutSine(GetPhase(ShotElapsed, ShotSeconds - CaptionFadeOutSeconds,
	                                             CaptionFadeOutSeconds));
	M_Widget->SetCaptionVisibility(FadeIn * (1.f - FadeOut));
}

void UMissionCameraTourSession::CompleteSession(const EMissionCameraTourEnd EndReason)
{
	if (bM_HasCompleted)
	{
		return;
	}
	const bool bHeldScreen = bM_IsRunning;
	bM_HasCompleted = true;
	bM_IsRunning = false;
	bM_IsWaitingForScreen = false;
	M_CurrentShotIndex = INDEX_NONE;
	RemoveSkipWidget();
	CloseOpeningTitle();

	if (bHeldScreen)
	{
		RestorePlayerView();
	}
	if (GetIsValidWidget(false))
	{
		// A line still playing moves back to the HUD portrait, which shows again below.
		ReleaseTourPortrait();
		M_Widget->CloseTour();
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
	M_Mission->OnCameraTourSessionEnded(this, M_ID, EndReason);
}

void UMissionCameraTourSession::RestorePlayerView()
{
	if (not bM_HasCameraPose || not GetIsValidCameraController(false))
	{
		return;
	}
	M_CameraController->ApplyScriptedCameraPose(M_PlayerPawnLocation,
	                                             FRotator(M_PlayerPose.Pitch, M_PlayerPose.Yaw, 0.f));
	M_CameraController->ApplyScriptedCameraArmLength(M_PlayerPose.ArmLength);
}

float UMissionCameraTourSession::GetElapsedSeconds() const
{
	const UWorld* const World = GetTickableGameObjectWorld();
	if (World == nullptr)
	{
		return 0.f;
	}
	return static_cast<float>(World->GetRealTimeSeconds() - M_StartRealSeconds);
}

bool UMissionCameraTourSession::GetIsValidPlayerController(const bool bReportError) const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	if (bReportError)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this, TEXT("M_PlayerController"), TEXT("UMissionCameraTourSession::GetIsValidPlayerController"), this);
	}
	return false;
}

bool UMissionCameraTourSession::GetIsValidCameraController(const bool bReportError) const
{
	if (M_CameraController.IsValid())
	{
		return true;
	}
	if (bReportError)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this, TEXT("M_CameraController"), TEXT("UMissionCameraTourSession::GetIsValidCameraController"), this);
	}
	return false;
}

bool UMissionCameraTourSession::GetIsValidSkipWidget(const bool bReportError) const
{
	if (IsValid(M_SkipWidget))
	{
		return true;
	}
	if (bReportError)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this, TEXT("M_SkipWidget"), TEXT("UMissionCameraTourSession::GetIsValidSkipWidget"), this);
	}
	return false;
}

bool UMissionCameraTourSession::GetIsValidWidget(const bool bReportError) const
{
	if (IsValid(M_Widget))
	{
		return true;
	}
	if (bReportError)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this, TEXT("M_Widget"), TEXT("UMissionCameraTourSession::GetIsValidWidget"), this);
	}
	return false;
}
