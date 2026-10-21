// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShortsDirector.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "EngineUtils.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MoviePipeline.h"
#include "MovieRenderPipelineDataTypes.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSubmix.h"
#include "Sound/SoundSubmix.h"
#include "AudioThread.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Environment/DestructableEnvActor/ApartmentDestruction/ApartmentDestructibleActor.h"
#include "NiagaraFunctionLibrary.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UObjectIterator.h"

#include "RTSShortsAudioCapture.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "RTSShortsRenderSubsystem.h"
#include "RTS_Survival/Game/GameState/HideGameUI/RTSUIElement.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/RTSCheatManager/RTSCheatManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShorts, Log, All);

namespace RTSShortsDirectorPrivate
{
	// Real seconds the HUD and start widget need before the start can be confirmed (same as the Molotov runner).
	constexpr double HudBuildSeconds = 4.0;
	constexpr double MaxStartSeconds = 60.0;
	constexpr double MaxTurretWaitSeconds = 20.0;
	constexpr uint8 ShortsPlayer = 1;
	constexpr float PenGridSpacing = 1500.f;
	constexpr int32 HoldFramesAfterCut = 12;
	constexpr float RangeUseFraction = 0.75f;
	constexpr int32 SoundLogEveryFrames = 15;

	const TCHAR* const MutedSoundClassPaths[] = {
		TEXT("/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_Music.RTS_SClass_Music"),
		TEXT("/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_UI.RTS_SClass_UI"),
		TEXT("/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_Announcer.RTS_SClass_Announcer"),
		TEXT("/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_VoiceLine.RTS_SClass_VoiceLine"),
		TEXT("/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_CinematicAndTransmissions.RTS_SClass_CinematicAndTransmissions"),
	};

	FName ReadNameProperty(const AActor* Actor, const TCHAR* PropertyName)
	{
		if (not IsValid(Actor))
		{
			return NAME_None;
		}
		const FNameProperty* Property = FindFProperty<FNameProperty>(Actor->GetClass(), PropertyName);
		if (Property == nullptr)
		{
			return NAME_None;
		}
		return Property->GetPropertyValue_InContainer(Actor);
	}

	bool GetClassNameStartsWith(const AActor* Actor, const TCHAR* Prefix)
	{
		return IsValid(Actor) && Actor->GetClass()->GetName().StartsWith(Prefix);
	}
}

TArray<ACPPTurretsMaster*> ARTSShortsDirector::GetUnitTurrets(const AActor* Unit)
{
	if (const ATankMaster* Tank = Cast<ATankMaster>(Unit))
	{
		return Tank->GetTurrets();
	}
	if (const ABuildingExpansion* Expansion = Cast<ABuildingExpansion>(Unit))
	{
		return Expansion->GetTurrets();
	}
	return {};
}

bool ARTSShortsDirector::GetHasTurretOwner(const AActor* Unit)
{
	return Cast<ATankMaster>(Unit) != nullptr || Cast<ABuildingExpansion>(Unit) != nullptr;
}

ARTSShortsDirector::ARTSShortsDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	// The start-game control pauses the world until the player presses start; the director confirms that.
	PrimaryActorTick.bTickEvenWhenPaused = true;
	// Late in the frame, so sounds started by this frame's gameplay are already queued when the spatialization
	// override below runs (see ForceSoundsNonSpatialized).
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetTickableWhenPaused(true);
}

void ARTSShortsDirector::InitShortsDirector(const FRTSShortsJob& Job)
{
	M_Job = Job;
	M_Manifest.JobName = Job.JobName;
	M_Manifest.Fps = Job.Fps;
	M_Manifest.WarmupFrames = FMath::RoundToInt(Job.WarmupSeconds * Job.Fps);
	M_Manifest.SequenceName = TEXT("LS_Shorts_") + Job.JobName;

	M_SegmentStartFrames.Reset();
	int32 Frame = M_Manifest.WarmupFrames;
	for (const FRTSShortsSegment& Segment : Job.Segments)
	{
		M_SegmentStartFrames.Add(Frame);
		Frame += FMath::RoundToInt(Segment.SlotSeconds * Job.Fps);
	}
	M_SegmentStartFrames.Add(Frame);
	M_Manifest.TotalFrames = Frame;

	M_Manifest.Segments.Reset();
	for (int32 Index = 0; Index < Job.Segments.Num(); ++Index)
	{
		FRTSShortsManifestSegment& Entry = M_Manifest.Segments.AddDefaulted_GetRef();
		Entry.Index = Index;
		Entry.Subtype = Job.Segments[Index].Subtype;
		Entry.Caption1 = Job.Segments[Index].Caption1;
		Entry.Caption2 = Job.Segments[Index].Caption2;
		Entry.StartFrame = M_SegmentStartFrames[Index];
		Entry.EndFrame = M_SegmentStartFrames[Index + 1];
	}
	M_Units.SetNum(Job.Segments.Num());
}

void ARTSShortsDirector::BeginPlay()
{
	Super::BeginPlay();
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_DIRECTOR_BEGIN job=%s segments=%d render=%s"),
	       *M_Job.JobName, M_Job.Segments.Num(), M_Job.bRender ? TEXT("true") : TEXT("false"));
	if (M_Job.bRender)
	{
		StartSubmixCapture();
	}
}

void ARTSShortsDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSubmixCapture();
	Super::EndPlay(EndPlayReason);
}

void ARTSShortsDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	KeepAudioUnmuted();
	if (M_Job.bRender)
	{
		ForceSoundsNonSpatialized();
	}
	switch (M_Phase)
	{
	case ERTSShortsDirectorPhase::WaitingForController: TickWaitingForController();
		break;
	case ERTSShortsDirectorPhase::WaitingForStartDialog: TickWaitingForStartDialog();
		break;
	case ERTSShortsDirectorPhase::SpawningUnits: TickSpawningUnits();
		break;
	case ERTSShortsDirectorPhase::WaitingForTurrets: TickWaitingForTurrets();
		break;
	case ERTSShortsDirectorPhase::Running: TickRunning();
		break;
	case ERTSShortsDirectorPhase::Finished:
		break;
	}
}

void ARTSShortsDirector::TickWaitingForController()
{
	if (not bM_HasSuppressedDialogs)
	{
		RTSFunctionLibrary::PushModalDialogSuppression();
		bM_HasSuppressedDialogs = true;
	}
	M_PlayerController = Cast<ACPPController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (not IsValid(M_PlayerController))
	{
		return;
	}
	if (not FindStageActors())
	{
		UE_LOG(LogRTSShorts, Error, TEXT("RTS_SHORTS_STAGE_MISSING stage=%s"), *M_Job.StageName);
		M_Phase = ERTSShortsDirectorPhase::Finished;
		WriteManifest();
		return;
	}
	M_Phase = ERTSShortsDirectorPhase::WaitingForStartDialog;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
}

void ARTSShortsDirector::TickWaitingForStartDialog()
{
	using namespace RTSShortsDirectorPrivate;
	if (not GetIsValidPlayerController())
	{
		return;
	}
	const double Waited = FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
	if (Waited < HudBuildSeconds)
	{
		return;
	}
	UPlayerStartGameControl* StartControl = M_PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	const bool bControlReady = IsValid(StartControl) && StartControl->HasBegunPlay();
	if (bControlReady && StartControl->GetIsWaitingForPlayerToStart())
	{
		StartControl->PlayerStartedGame();
		UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_START_CONFIRMED after=%.1fs"), Waited);
	}
	const bool bStarted = bControlReady && not StartControl->GetIsWaitingForPlayerToStart();
	if (not bStarted && Waited < MaxStartSeconds)
	{
		return;
	}
	if (not bStarted)
	{
		UE_LOG(LogRTSShorts, Warning, TEXT("RTS_SHORTS_START_TIMEOUT continuing without confirmation"));
	}
	HideGameUI();
	PushShortsSoundMix();
	LockViewOnCamera();
	ApplyStageVisibility();
	M_Phase = ERTSShortsDirectorPhase::SpawningUnits;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
}

void ARTSShortsDirector::TickSpawningUnits()
{
	for (int32 Index = 0; Index < M_Job.Segments.Num(); ++Index)
	{
		M_Units[Index] = SpawnSegmentUnit(Index);
	}
	M_Phase = ERTSShortsDirectorPhase::WaitingForTurrets;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
}

void ARTSShortsDirector::TickWaitingForTurrets()
{
	using namespace RTSShortsDirectorPrivate;
	bool bAllReady = true;
	for (int32 Index = 0; Index < M_Units.Num(); ++Index)
	{
		const AActor* Unit = M_Units[Index].Get();
		if (IsValid(Unit) && GetHasTurretOwner(Unit) && GetUnitTurrets(Unit).Num() == 0)
		{
			bAllReady = false;
		}
	}
	const double Waited = FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
	if (not bAllReady && Waited < MaxTurretWaitSeconds)
	{
		return;
	}
	for (int32 Index = 0; Index < M_Units.Num(); ++Index)
	{
		const AActor* Unit = M_Units[Index].Get();
		if (IsValid(Unit) && GetHasTurretOwner(Unit) && GetUnitTurrets(Unit).Num() == 0)
		{
			MarkSegmentSkipped(Index, TEXT("turrets never became ready"));
		}
	}
	DisableUnitOptimisation();
	ParkStartingUnits();
	for (int32 Index = 0; Index < M_Units.Num(); ++Index)
	{
		if (AActor* Unit = M_Units[Index].Get())
		{
			ForceAnimationTicking(Unit);
		}
	}
	// The first vehicle goes on its mark during the warm-up: placed on the cut frame, the BT-7 Soviet stayed in its
	// pen for 12 frames and never fired. Squads keep waiting in the pen (their members follow the controller).
	AActor* FirstUnit = M_Units.IsValidIndex(0) ? M_Units[0].Get() : nullptr;
	if (IsValid(FirstUnit) && Cast<ASquadController>(FirstUnit) == nullptr && M_Job.Segments.IsValidIndex(0))
	{
		PlaceUnitOnStage(FirstUnit, M_Job.Segments[0]);
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_READY units=%d waited=%.1fs allReady=%s"),
	       M_Units.Num(), Waited, bAllReady ? TEXT("true") : TEXT("false"));
	M_RunStartWorldSeconds = GetWorld()->GetTimeSeconds();
	M_Phase = ERTSShortsDirectorPhase::Running;
	if (M_Job.bRecordAudio && not M_Job.bRender)
	{
		BeginRealtimeAudioRecording();
	}
	WriteManifest();
}

void ARTSShortsDirector::TickRunning()
{
	const int32 Frame = GetTimelineFrame();
	if (Frame < 0)
	{
		return;
	}
	UpdateSubmixCaptureWindow(Frame);
	// Advance to the segment this frame belongs to; also ends segments whose slot ran out.
	int32 TargetSegment = -1;
	for (int32 Index = 0; Index < M_Job.Segments.Num(); ++Index)
	{
		if (Frame >= M_SegmentStartFrames[Index])
		{
			TargetSegment = Index;
		}
	}
	if (Frame >= M_SegmentStartFrames.Last())
	{
		TargetSegment = M_Job.Segments.Num();
	}
	while (M_CurrentSegment < TargetSegment)
	{
		if (M_CurrentSegment >= 0)
		{
			EndSegment(M_CurrentSegment);
		}
		++M_CurrentSegment;
		if (M_CurrentSegment < M_Job.Segments.Num())
		{
			BeginSegment(M_CurrentSegment);
		}
	}
	if (M_CurrentSegment >= M_Job.Segments.Num())
	{
		M_Manifest.bComplete = true;
		M_Phase = ERTSShortsDirectorPhase::Finished;
		WriteManifest();
		UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_DIRECTOR_DONE frames=%d"), Frame);
		if (bM_TimelineShortened)
		{
			// Battles ended early: nothing left to film, so the movie render stops here instead of at the planned end.
			if (UMoviePipeline* Pipeline = GetActivePipeline())
			{
				Pipeline->RequestShutdown();
				UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_RENDER_STOPPED_EARLY frame=%d"), Frame);
			}
		}
		if (M_Job.bRecordAudio && not M_Job.bRender)
		{
			EndRealtimeAudioRecordingAndExit();
		}
		return;
	}
	if (M_CurrentSegment < 0)
	{
		// Still inside the warm-up slot: the stage stays empty until the first segment's start frame.
		if (Frame % RTSShortsDirectorPrivate::SoundLogEveryFrames == 0)
		{
			LogActiveSounds(Frame);
		}
		return;
	}
	const FRTSShortsSegment& Segment = M_Job.Segments[M_CurrentSegment];
	const int32 Local = Frame - M_SegmentStartFrames[M_CurrentSegment];
	if (M_Job.bAudioProbe && M_CurrentSegment == 0)
	{
		RunAudioProbe(Local);
	}
	if (Local % RTSShortsDirectorPrivate::SoundLogEveryFrames == 0)
	{
		LogActiveSounds(Frame);
	}
	if (M_HoldFramesLeft > 0)
	{
		// Re-apply the placement for a few frames so vehicle physics cannot slide the unit into frame.
		--M_HoldFramesLeft;
		AActor* Unit = M_Units[M_CurrentSegment].Get();
		if (IsValid(Unit) && Cast<ASquadController>(Unit) == nullptr)
		{
			PlaceUnitOnStage(Unit, Segment);
			M_PlacedLocation = Unit->GetActorLocation();
		}
	}
	else if (AActor* Unit = M_Units[M_CurrentSegment].Get())
	{
		// Some units drive toward their attack-ground target (into the camera); a unit that left its mark by
		// more than half a metre is put back where it was placed.
		constexpr float MaxDriftCm = 50.f;
		constexpr float MaxLiftCm = 120.f;
		const bool bDrifted = FVector::Dist2D(Unit->GetActorLocation(), M_PlacedLocation) > MaxDriftCm
			|| Unit->GetActorLocation().Z - M_PlacedLocation.Z > MaxLiftCm;
		if (Cast<ASquadController>(Unit) == nullptr && bDrifted)
		{
			UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_DRIFT_RESET segment=%d frame=%d placed=%s now=%s dist=%.0f"),
			       M_CurrentSegment, Frame, *M_PlacedLocation.ToCompactString(), *Unit->GetActorLocation().ToCompactString(),
			       FVector::Dist2D(Unit->GetActorLocation(), M_PlacedLocation));
			// A unit whose first placement did not take (the BT-7 Soviet stayed in its pen for 12 frames) was framed
			// where it stood: re-frame the camera once it really arrives on the stage.
			constexpr float ReframeAfterJumpCm = 1000.f;
			const bool bArrivedFromAfar = FVector::Dist2D(Unit->GetActorLocation(), M_Stage->GetActorLocation()) > ReframeAfterJumpCm;
			PlaceUnitOnStage(Unit, Segment);
			M_PlacedLocation = Unit->GetActorLocation();
			if (bArrivedFromAfar)
			{
				FrameCamera(Unit, Segment, GetTargetWorldLocation(Segment, Unit));
			}
		}
	}
	if (Local % 15 == 0 && Local <= 120)
	{
		// Squads: where did the visible soldier go after the cut (formation walk, range closing)?
		if (const ASquadController* Squad = Cast<ASquadController>(M_Units[M_CurrentSegment].Get()))
		{
			const AActor* Lead = GetFramingSubject(Squad);
			const ASquadUnit* LeadSoldier = Cast<ASquadUnit>(Lead);
			const AInfantryWeaponMaster* LeadWeapon = LeadSoldier ? LeadSoldier->GetInfantryWeapon() : nullptr;
			UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_SQUAD_LEAD segment=%d local=%d lead=%s weapon=%s loc=%s"), M_CurrentSegment, Local,
			       *Lead->GetName(), IsValid(LeadWeapon) ? *LeadWeapon->GetClass()->GetName() : TEXT("none"), *Lead->GetActorLocation().ToCompactString());
		}
	}
	if (AActor* DecalUnit = M_Units[M_CurrentSegment].Get())
	{
		SuppressUnitDecals(DecalUnit, M_CurrentSegment);
		LimitStageEffects(DecalUnit, Segment, Frame);
	}
	if (Segment.bDisarmTarget)
	{
		DisarmSegmentTarget();
	}
	if (Frame % 30 == 0)
	{
		// Waiting squads: how many members each still has and where the first one is (diagnoses squads that
		// arrive at their slot empty).
		for (int32 Index = M_CurrentSegment + 1; Index < M_Units.Num(); ++Index)
		{
			if (ASquadController* Waiting = Cast<ASquadController>(M_Units[Index].Get()))
			{
				const TArray<ASquadUnit*> Members = Waiting->GetSquadUnitsChecked();
				const ASquadUnit* First = Members.Num() > 0 ? Members[0] : nullptr;
				UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_WAITING_SQUAD frame=%d segment=%d members=%d first=%s"), Frame, Index,
				       Members.Num(), IsValid(First) ? *First->GetActorLocation().ToCompactString() : TEXT("none"));
			}
		}
	}
	LogVisibleSoldierShots(Frame);
	UpdateOrbitCamera(static_cast<float>(Local) / M_Job.Fps);
	TickBattle(Frame);
	if (M_HiddenSoldiers.IsValidIndex(M_CurrentSegment))
	{
		// Hidden squad members stay silent: their shots would be heard but not seen.
		for (const TWeakObjectPtr<ASquadUnit>& Hidden : M_HiddenSoldiers[M_CurrentSegment])
		{
			if (Hidden.IsValid())
			{
				if (AInfantryWeaponMaster* Weapon = Hidden->GetInfantryWeapon())
				{
					Weapon->DisableWeaponSearch(false, true);
				}
			}
		}
	}
	const float StopAt = Segment.StopAtSeconds >= 0.f ? Segment.StopAtSeconds : Segment.SlotSeconds - 0.5f;
	if (not bM_FireOrdered && Local >= FMath::RoundToInt(Segment.FireAtSeconds * M_Job.Fps))
	{
		OrderAttackGround(M_CurrentSegment);
		if (Segment.StopAfterFireSeconds >= 0.f)
		{
			M_StopAfterFireFrame = Local + FMath::RoundToInt(Segment.StopAfterFireSeconds * M_Job.Fps);
		}
	}
	// Single shots: a stop order shortly after each fire order, then (optionally) a second fire order.
	if (M_StopAfterFireFrame >= 0 && Local >= M_StopAfterFireFrame)
	{
		OrderStop(M_CurrentSegment);
		bM_StopOrdered = false;
		M_StopAfterFireFrame = -1;
	}
	if (Segment.FireAgainAtSeconds >= 0.f && not bM_SecondFireOrdered
		&& Local >= FMath::RoundToInt(Segment.FireAgainAtSeconds * M_Job.Fps))
	{
		bM_SecondFireOrdered = true;
		OrderAttackGround(M_CurrentSegment);
		if (Segment.StopAfterFireSeconds >= 0.f)
		{
			M_StopAfterFireFrame = Local + FMath::RoundToInt(Segment.StopAfterFireSeconds * M_Job.Fps);
		}
	}
	if (not bM_StopOrdered && Local >= FMath::RoundToInt(StopAt * M_Job.Fps))
	{
		OrderStop(M_CurrentSegment);
	}
	const bool bImpactCutDue = Segment.ImpactCutAtSeconds >= 0.f
		&& Local >= FMath::RoundToInt(Segment.ImpactCutAtSeconds * M_Job.Fps);
	if (not bM_ImpactCutDone && bImpactCutDue)
	{
		bM_ImpactCutDone = true;
		FrameImpactCamera(M_Units[M_CurrentSegment].Get(), Segment);
	}
}

void ARTSShortsDirector::ApplyCameraCut(const FVector& CameraLocation) const
{
	if (not GetIsValidPlayerController())
	{
		return;
	}
	if (IsValid(M_PlayerController->PlayerCameraManager))
	{
		M_PlayerController->PlayerCameraManager->SetGameCameraCutThisFrame();
		// The controller already updated its view this frame; refresh it so the frame rendered at the end of this
		// tick uses the new camera transform instead of showing the previous cut for one frame.
		M_PlayerController->PlayerCameraManager->UpdateCamera(0.f);
	}
	// Distance based unit optimisation measures from the player's pawn, so it follows the shorts camera.
	if (APawn* Pawn = M_PlayerController->GetPawn())
	{
		Pawn->SetActorLocation(CameraLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ARTSShortsDirector::FrameImpactCamera(const AActor* Unit, const FRTSShortsSegment& Segment) const
{
	if (not GetIsValidCamera() || not IsValid(Unit))
	{
		return;
	}
	// The impact cut replaces the segment's camera, orbit pan included.
	M_Orbit.bIsActive = false;
	const FRTSShortsImpactCameraParams& Params = Segment.ImpactCamera;
	const FVector Target = GetTargetWorldLocation(Segment, Unit);
	const FVector UnitLocation = Unit->GetActorLocation();
	const FVector LineDir = (Target - UnitLocation).GetSafeNormal2D();
	FVector CameraLocation;
	if (Params.bOverShoulder)
	{
		const FVector Right = LineDir.RotateAngleAxis(90.f, FVector::UpVector);
		CameraLocation = UnitLocation - LineDir * Params.BehindDistance + Right * Params.SideOffset;
		// Same ground reference as camera A: the placement location, not the bounds (gun pits and casemate
		// vehicles carry oversized bounds that put a bounds-based camera under the slab).
		CameraLocation.Z = UnitLocation.Z + Params.Height;
	}
	else
	{
		const FVector OffsetDir = LineDir.RotateAngleAxis(Params.AngleDeg, FVector::UpVector);
		CameraLocation = Target + OffsetDir * Params.Distance;
		CameraLocation.Z = Target.Z + Params.Height;
	}
	FVector LookAt = FMath::Lerp(Target, UnitLocation, Params.UnitLookBlend);
	LookAt.Z = Target.Z + Params.LookAtHeight;
	FRotator Rotation = (LookAt - CameraLocation).Rotation();
	Rotation.Roll = 0.f;
	M_Camera->SetActorLocationAndRotation(CameraLocation, Rotation);
	if (UCineCameraComponent* CineComponent = M_Camera->GetCineCameraComponent())
	{
		CineComponent->SetCurrentFocalLength(Params.FocalLength);
		// No depth of field: the default manual focus distance blurred small units framed a few metres away.
		CineComponent->FocusSettings.FocusMethod = ECameraFocusMethod::Disable;
		CineComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
		CineComponent->PostProcessSettings.AutoExposureBias = Params.ExposureBias;
		// No motion blur: at 60 fps it smeared rifles into unreadable streaks.
		CineComponent->PostProcessSettings.bOverride_MotionBlurAmount = true;
		CineComponent->PostProcessSettings.MotionBlurAmount = 0.f;
		CineComponent->PostProcessSettings.bOverride_ColorSaturation = true;
		CineComponent->PostProcessSettings.ColorSaturation = FVector4(Params.ColorSaturation, Params.ColorSaturation, Params.ColorSaturation, 1.f);
		CineComponent->PostProcessBlendWeight = 1.f;
	}
	ApplyCameraCut(CameraLocation);
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_IMPACT_CUT frame=%d"), GetTimelineFrame());
}

bool ARTSShortsDirector::FindStageActors()
{
	using namespace RTSShortsDirectorPrivate;
	const FName WantedStage(*M_Job.StageName);
	const FString WantedActorName = TEXT("Stage_") + (M_Job.StageActorName.IsEmpty() ? M_Job.StageName : M_Job.StageActorName);
	for (TActorIterator<ACineCameraActor> CameraIt(GetWorld()); CameraIt; ++CameraIt)
	{
		M_Camera = *CameraIt;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (GetClassNameStartsWith(Actor, TEXT("BP_ShortsStage")))
		{
			ConsiderStageActor(Actor, WantedActorName, WantedStage);
		}
		else if (GetClassNameStartsWith(Actor, TEXT("BP_ShortsPen")))
		{
			M_Pen = Actor;
		}
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_STAGE stage=%s at=%s camera=%s"),
	       IsValid(M_Stage) ? *M_Stage->GetName() : TEXT("none"),
	       IsValid(M_Stage) ? *M_Stage->GetActorLocation().ToString() : TEXT(""),
	       IsValid(M_Camera) ? *M_Camera->GetName() : TEXT("none"));
	return IsValid(M_Stage) && IsValid(M_Camera);
}

void ARTSShortsDirector::ConsiderStageActor(AActor* Actor, const FString& WantedActorName, const FName WantedStage)
{
	using namespace RTSShortsDirectorPrivate;
	// The placed actor's object name is its editor label ("Stage_Vehicle"), which beats the StageName variable.
	bool bNameMatches = Actor->GetName() == WantedActorName;
#if WITH_EDITOR
	// A stage duplicated from script keeps a generated object name; its label still says which stage it is.
	bNameMatches = bNameMatches || Actor->GetActorLabel() == WantedActorName;
#endif
	if (bNameMatches)
	{
		M_Stage = Actor;
		bM_StageMatchedByName = true;
		return;
	}
	if (bM_StageMatchedByName || ReadNameProperty(Actor, TEXT("StageName")) != WantedStage)
	{
		return;
	}
	// Several stages can carry the same StageName when an instance edit did not persist: keep the one nearest
	// the placed ShortsCamera, which the level author put next to the intended stage.
	const AActor* Camera = M_Camera.Get();
	if (not IsValid(M_Stage) || not IsValid(Camera))
	{
		M_Stage = Actor;
		return;
	}
	const float CurrentDistance = FVector::Dist(M_Stage->GetActorLocation(), Camera->GetActorLocation());
	if (FVector::Dist(Actor->GetActorLocation(), Camera->GetActorLocation()) < CurrentDistance)
	{
		M_Stage = Actor;
	}
}

void ARTSShortsDirector::HideGameUI() const
{
	int32 Toggled = 0;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Object = *It;
		const bool bCandidate = IsValid(Object) && not Object->IsTemplate() && Object->GetWorld() == GetWorld();
		if (not bCandidate || not Object->GetClass()->ImplementsInterface(URTSUIElement::StaticClass()))
		{
			continue;
		}
		if (IRTSUIElement* Element = Cast<IRTSUIElement>(Object))
		{
			Element->OnHideAllGameUI(true);
			++Toggled;
		}
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_UI_HIDDEN elements=%d"), Toggled);
}

void ARTSShortsDirector::PushShortsSoundMix()
{
	using namespace RTSShortsDirectorPrivate;
	M_SoundMix = NewObject<USoundMix>(this, TEXT("RTS_SMix_Shorts"));
	for (const TCHAR* Path : MutedSoundClassPaths)
	{
		USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, Path);
		if (not IsValid(SoundClass))
		{
			UE_LOG(LogRTSShorts, Warning, TEXT("RTS_SHORTS_SOUNDCLASS_MISSING %s"), Path);
			continue;
		}
		FSoundClassAdjuster& Adjuster = M_SoundMix->SoundClassEffects.AddDefaulted_GetRef();
		Adjuster.SoundClassObject = SoundClass;
		Adjuster.VolumeAdjuster = 0.f;
		Adjuster.bApplyToChildren = true;
	}
	UGameplayStatics::PushSoundMixModifier(GetWorld(), M_SoundMix);
}

void ARTSShortsDirector::LockViewOnCamera() const
{
	if (not GetIsValidPlayerController() || not GetIsValidCamera())
	{
		return;
	}
	M_PlayerController->SetViewTargetWithBlend(M_Camera, 0.f);
}

AActor* ARTSShortsDirector::SpawnSegmentUnit(const int32 SegmentIndex)
{
	using namespace RTSShortsDirectorPrivate;
	const FRTSShortsSegment& Segment = M_Job.Segments[SegmentIndex];
	if (Segment.ClassPath.IsEmpty())
	{
		MarkSegmentSkipped(SegmentIndex, TEXT("no ClassPath"));
		return nullptr;
	}
	TSet<AActor*> TeamWeaponsBefore;
	for (TActorIterator<ATeamWeapon> It(GetWorld()); It; ++It)
	{
		TeamWeaponsBefore.Add(*It);
	}
	AActor* Unit = nullptr;
	if (Segment.Kind.Equals(TEXT("Pit"), ESearchCase::IgnoreCase))
	{
		Unit = SpawnPitAsPlaced(Segment.ClassPath, GetPenTransform(SegmentIndex));
	}
	else
	{
#if RTS_WITH_MAP_TESTS
		Unit = FGrenadeTestHelpers::SpawnUnitForPlayer(*GetWorld(), *Segment.ClassPath,
		                                                GetPenTransform(SegmentIndex), static_cast<uint8>(M_Job.UnitOwningPlayer));
#endif
	}
	// A team-weapon squad spawns its gun as a separate actor; keep it so the placement moves gun and crew together.
	if (M_SegmentTeamWeapons.Num() < M_Job.Segments.Num())
	{
		M_SegmentTeamWeapons.SetNum(M_Job.Segments.Num());
	}
	for (TActorIterator<ATeamWeapon> It(GetWorld()); It; ++It)
	{
		if (not TeamWeaponsBefore.Contains(*It))
		{
			M_SegmentTeamWeapons[SegmentIndex] = *It;
		}
	}
	if (not IsValid(Unit))
	{
		MarkSegmentSkipped(SegmentIndex, TEXT("spawn failed: ") + Segment.ClassPath);
		return nullptr;
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_SPAWNED index=%d unit=%s"), SegmentIndex, *Unit->GetName());
	return Unit;
}

void ARTSShortsDirector::ForceAnimationTicking(AActor* Unit) const
{
	// Offscreen rendering does not mark meshes as recently rendered, so crews and guns whose shot sound comes
	// from an animation notify would stay silent with OnlyTickPoseWhenRendered.
	TArray<AActor*> Actors;
	Actors.Add(Unit);
	Unit->GetAttachedActors(Actors, false, true);
	int32 Count = 0;
	for (AActor* Actor : Actors)
	{
		TArray<USkeletalMeshComponent*> Meshes;
		Actor->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			++Count;
		}
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_ANIM_TICK_FORCED unit=%s meshes=%d actors=%d"),
	       *Unit->GetName(), Count, Actors.Num());
}

void ARTSShortsDirector::ParkStartingUnits() const
{
	// The player profile spawns starting units (HQ truck and friends) at the player start, 16 m behind the stage
	// and inside the hero shot. Move every tank that is not one of ours to the far side of the pen.
	// They go 40 m behind the pen grid (its rows grow forward): parked on the grid they landed on the segment 2-3
	// slots and crushed the squads waiting there.
	constexpr float StartingUnitsBehindPenCm = 4000.f;
	constexpr float StartingUnitsSpacingCm = 1200.f;
	constexpr float StartingUnitsLiftCm = 50.f;
	const FTransform Pen = GetPenTransform(0);
	const FVector PenForward = Pen.GetRotation().GetForwardVector();
	const FVector PenRight = Pen.GetRotation().GetRightVector();
	int32 Parked = 0;
	for (TActorIterator<ATankMaster> It(GetWorld()); It; ++It)
	{
		ATankMaster* Tank = *It;
		if (not IsValid(Tank) || M_Units.Contains(Tank))
		{
			continue;
		}
		const FVector Location = Pen.GetLocation() - PenForward * StartingUnitsBehindPenCm
			+ PenRight * (StartingUnitsSpacingCm * Parked) + FVector(0.f, 0.f, StartingUnitsLiftCm);
		Tank->SetActorLocationAndRotation(Location, Pen.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
		++Parked;
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_STARTING_UNITS_PARKED count=%d"), Parked);
}

void ARTSShortsDirector::DisableUnitOptimisation() const
{
	if (not GetIsValidPlayerController())
	{
		return;
	}
	if (not IsValid(M_PlayerController->CheatManager))
	{
		M_PlayerController->EnableCheats();
	}
	URTSCheatManager* Cheats = Cast<URTSCheatManager>(M_PlayerController->CheatManager);
	if (not IsValid(Cheats))
	{
		UE_LOG(LogRTSShorts, Warning, TEXT("RTS_SHORTS_NO_CHEAT_MANAGER optimisation stays on"));
		return;
	}
	Cheats->RTSOptimizeUnits(false);
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_UNIT_OPTIMISATION_OFF"));
}

AActor* ARTSShortsDirector::SpawnPitAsPlaced(const FString& ClassPath, const FTransform& Transform) const
{
	// A gun pit spawned with Blueprint defaults is already built, like one a level designer placed.
	UClass* PitClass = LoadClass<AActor>(nullptr, *ClassPath);
	if (PitClass == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<AActor>(PitClass, Transform, Params);
}

FTransform ARTSShortsDirector::GetPenTransform(const int32 SegmentIndex) const
{
	using namespace RTSShortsDirectorPrivate;
	// Pens sit 150 m behind the stage camera side (the stage's forward axis), well outside every setup's view;
	// a placed pen actor is only honoured when it lies within 100 m of the stage.
	const FRotator StageRotation = M_Stage->GetActorRotation();
	const FVector StageForward = StageRotation.Vector();
	const FVector StageRight = FRotationMatrix(StageRotation).GetUnitAxis(EAxis::Y);
	// 80 m to the side as well: a direct-fire gun's rounds overshoot the ground target along the stage axis
	// and killed a parked mortar crew at 150 m.
	// Job.PenLocalOffset moves them (stage-local X forward, Y right), e.g. behind a camera that looks downrange.
	FVector Origin = M_Stage->GetActorLocation() + StageForward * M_Job.PenLocalOffset.X + StageRight * M_Job.PenLocalOffset.Y;
	// A job that sets its own PenLocalOffset keeps it even when a placed pen actor happens to be near the stage.
	const bool bJobPen = not M_Job.PenLocalOffset.Equals(FVector(15000.f, 8000.f, 0.f), 1.f);
	if (not bJobPen && IsValid(M_Pen) && FVector::Dist2D(M_Pen->GetActorLocation(), M_Stage->GetActorLocation()) < 10000.f)
	{
		Origin = M_Pen->GetActorLocation();
	}
	const int32 Column = SegmentIndex % 4;
	const int32 Row = SegmentIndex / 4;
	const FVector Offset = StageForward * (Row * PenGridSpacing) + StageRight * ((Column - 1.5f) * PenGridSpacing);
	return FTransform(StageRotation, Origin + Offset);
}

int32 ARTSShortsDirector::GetTimelineFrame() const
{
	if (not M_Job.bRender)
	{
		return FMath::FloorToInt((GetWorld()->GetTimeSeconds() - M_RunStartWorldSeconds) * M_Job.Fps);
	}
	const UMoviePipeline* Pipeline = GetActivePipeline();
	if (not IsValid(Pipeline))
	{
		return -1;
	}
	return Pipeline->GetOutputState().OutputFrameNumber;
}

UMoviePipeline* ARTSShortsDirector::GetActivePipeline() const
{
	const UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	if (not IsValid(GameInstance))
	{
		return nullptr;
	}
	const URTSShortsRenderSubsystem* Subsystem = GameInstance->GetSubsystem<URTSShortsRenderSubsystem>();
	return IsValid(Subsystem) ? Subsystem->GetActivePipeline() : nullptr;
}

void ARTSShortsDirector::FlushStageEffects(const int32 SegmentIndex) const
{
	// Smoke and dust of the previous unit must not linger behind the next one: stop every Niagara and Cascade
	// system that does not belong to the unit about to go on stage.
	const AActor* Keep = M_Units.IsValidIndex(SegmentIndex) ? M_Units[SegmentIndex].Get() : nullptr;
	int32 Stopped = 0;
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* Component = *It;
		// Level-placed actors (burning wrecks, ambient effects) keep running; only runtime-spawned effects are flushed.
		const AActor* ComponentOwner = Component->GetOwner();
		if (Component->GetWorld() != GetWorld() || (Keep != nullptr && ComponentOwner == Keep)
			|| (ComponentOwner != nullptr && ComponentOwner->IsNetStartupActor()))
		{
			continue;
		}
		if (Component->IsActive())
		{
			Component->DeactivateImmediate();
			++Stopped;
		}
	}
	for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
	{
		UParticleSystemComponent* Component = *It;
		// Level-placed actors (burning wrecks, ambient effects) keep running; only runtime-spawned effects are flushed.
		const AActor* ComponentOwner = Component->GetOwner();
		if (Component->GetWorld() != GetWorld() || (Keep != nullptr && ComponentOwner == Keep)
			|| (ComponentOwner != nullptr && ComponentOwner->IsNetStartupActor()))
		{
			continue;
		}
		if (Component->IsActive())
		{
			Component->DeactivateImmediate();
			++Stopped;
		}
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_EFFECTS_FLUSHED segment=%d stopped=%d"), SegmentIndex, Stopped);
}

void ARTSShortsDirector::BeginSegment(const int32 SegmentIndex)
{
	FlushStageEffects(SegmentIndex);
	// Nothing on stage may carry the selection outline or decal.
	if (GetIsValidPlayerController())
	{
		M_PlayerController->ResetSelectionArrays();
	}
	bM_FireOrdered = false;
	M_LastVisibleRounds = -1;
	bM_StopOrdered = false;
	bM_SecondFireOrdered = false;
	M_StopAfterFireFrame = -1;
	bM_ImpactCutDone = false;
	M_HoldFramesLeft = RTSShortsDirectorPrivate::HoldFramesAfterCut;
	const FRTSShortsSegment& Segment = M_Job.Segments[SegmentIndex];
	AActor* Unit = M_Units[SegmentIndex].Get();
	ASquadController* WaitingSquad = Cast<ASquadController>(Unit);
	const bool bSquadDied = Unit != nullptr && (not IsValid(Unit)
		|| (IsValid(WaitingSquad) && WaitingSquad->GetSquadUnitsChecked().Num() == 0));
	if (bSquadDied && not M_Manifest.Segments[SegmentIndex].bSkipped)
	{
		// Some squads lose every member while they wait in the pen (a PTRS crew was gone before the first segment
		// wherever the pen was; its controller then destroys itself); a fresh squad spawns its members at once and is
		// placed below like any other.
		UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_SQUAD_RESPAWNED segment=%d old=%s"), SegmentIndex, *Unit->GetName());
		if (IsValid(Unit))
		{
			Unit->Destroy();
		}
		M_Units[SegmentIndex] = SpawnSegmentUnit(SegmentIndex);
		Unit = M_Units[SegmentIndex].Get();
		if (IsValid(Unit))
		{
			DisableUnitOptimisation();
			ForceAnimationTicking(Unit);
		}
	}
	const FVector TargetWorld = GetTargetWorldLocation(Segment, Unit);
	M_SegmentTarget = nullptr;
	if (not Segment.TargetClassPath.IsEmpty())
	{
#if RTS_WITH_MAP_TESTS
		const FRotator FaceBack(0.f, M_Stage->GetActorRotation().Yaw + Segment.UnitYawOffset + 180.f, 0.f);
		UClass* TargetClass = LoadClass<AActor>(nullptr, *Segment.TargetClassPath);
		if (IsValid(TargetClass) && TargetClass->IsChildOf(AApartmentDestructibleActor::StaticClass()))
		{
			// A building: spawned deferred so its part health can be scaled before the parts are set up (a short
			// slot has room for only a few rounds), with a wider splash so each hit takes out more of it.
			const FTransform TargetTransform(FaceBack, TargetWorld);
			AActor* Building = GetWorld()->SpawnActorDeferred<AActor>(TargetClass, TargetTransform, nullptr, nullptr,
			                                                         ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (IsValid(Building))
			{
				if (FStructProperty* SettingsProperty = FindFProperty<FStructProperty>(AApartmentDestructibleActor::StaticClass(), TEXT("DestructionSettings")))
				{
					FApartmentDestructionSettings* Settings = SettingsProperty->ContainerPtrToValuePtr<FApartmentDestructionSettings>(Building);
					const float HealthScale = FMath::Max(0.01f, Segment.TargetHealthScale);
					FApartmentPartHealth& Health = Settings->PartHealth;
					Health.ExteriorWall *= HealthScale;
					Health.InteriorWall *= HealthScale;
					Health.Floor *= HealthScale;
					Health.Roof *= HealthScale;
					Health.Stairs *= HealthScale;
					Health.Detail *= HealthScale;
					Settings->SplashRadius *= Segment.TargetSplashScale;
				}
				Building->FinishSpawning(TargetTransform);
			}
			M_SegmentTarget = Building;
		}
		else
		{
			M_SegmentTarget = FGrenadeTestHelpers::SpawnUnitForPlayer(*GetWorld(), *Segment.TargetClassPath,
			                                                           FTransform(FaceBack, TargetWorld + FVector(0.f, 0.f, 50.f)),
			                                                           static_cast<uint8>(Segment.TargetOwningPlayer));
		}
#endif
		UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_TARGET_SPAWNED segment=%d class=%s ok=%d"), SegmentIndex,
		       *Segment.TargetClassPath, M_SegmentTarget.IsValid() ? 1 : 0);
	}
	if (IsValid(Unit))
	{
		PlaceUnitOnStage(Unit, Segment);
		if (Segment.bSwitchWeapons)
		{
			if (ICommands* Commands = Cast<ICommands>(Unit))
			{
				const ECommandQueueError Result = Commands->SwitchWeapons(true);
				UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_SWITCH_WEAPONS segment=%d result=%d"), SegmentIndex, static_cast<int32>(Result));
			}
		}
		SuppressUnitDecals(Unit, SegmentIndex);
		ApplyInfantryFireInterval(Unit, Segment);
		M_Manifest.Segments[SegmentIndex].CaliberMm = ReadLargestCaliberMm(Unit);
		FrameCamera(Unit, Segment, TargetWorld);
		if (Segment.BattleEnemyClassPaths.Num() > 0)
		{
			BeginBattle(SegmentIndex, Unit);
		}
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_SEGMENT_BEGIN index=%d subtype=%s frame=%d caliber=%d range=%.0f target=%s"),
	       SegmentIndex, *Segment.Subtype, GetTimelineFrame(), M_Manifest.Segments[SegmentIndex].CaliberMm,
	       ReadLargestWeaponRange(Unit), *TargetWorld.ToString());
	WriteManifest();
}

void ARTSShortsDirector::EndSegment(const int32 SegmentIndex)
{
	EndBattle();
	if (not bM_StopOrdered)
	{
		OrderStop(SegmentIndex);
	}
	AActor* Unit = M_Units[SegmentIndex].Get();
	if (IsValid(Unit))
	{
		ParkUnit(Unit, SegmentIndex);
	}
	if (M_SegmentTarget.IsValid())
	{
		if (ASquadController* TargetSquad = Cast<ASquadController>(M_SegmentTarget.Get()))
		{
			// The soldiers are separate actors: destroying only the controller left armed orphans by the stage that
			// fought the next segments' squads.
			// A copy: each destroyed soldier removes itself from the squad's own array.
			TArray<TWeakObjectPtr<ASquadUnit>> Soldiers;
			for (ASquadUnit* Soldier : TargetSquad->GetSquadUnitsChecked())
			{
				Soldiers.Add(Soldier);
			}
			for (const TWeakObjectPtr<ASquadUnit>& Soldier : Soldiers)
			{
				if (Soldier.IsValid())
				{
					Soldier->Destroy();
				}
			}
		}
		// A squad controller destroys itself with its last soldier.
		if (AActor* TargetActor = M_SegmentTarget.Get(); IsValid(TargetActor))
		{
			TargetActor->Destroy();
		}
		M_SegmentTarget = nullptr;
	}
}

void ARTSShortsDirector::PlaceUnitOnStage(AActor* Unit, const FRTSShortsSegment& Segment) const
{
	const FTransform StageTransform = M_Stage->GetActorTransform();
	const FRotator Rotation(0.f, StageTransform.Rotator().Yaw + Segment.UnitYawOffset, 0.f);
	FVector Location = StageTransform.GetLocation();
	FHitResult Hit;
	const FVector Start = Location + FVector(0.f, 0.f, 2000.f);
	const FVector End = Location - FVector(0.f, 0.f, 2000.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShortsPlace), false, Unit);
	// The previous unit was parked this same frame and its collision can still stand on the mark: the KV-2 after the
	// Maus landed on the Maus's old roof, 3 m up, and fell into frame two seconds late.
	for (const TObjectPtr<AActor>& OtherUnit : M_Units)
	{
		if (not IsValid(OtherUnit))
		{
			continue;
		}
		Params.AddIgnoredActor(OtherUnit.Get());
		TArray<AActor*> AttachedActors;
		OtherUnit->GetAttachedActors(AttachedActors, true, true);
		Params.AddIgnoredActors(AttachedActors);
	}
	// Height above the ground the unit rests at where it stands now (settled in its pen, or already on the stage):
	// placing it at that height means it lands resting instead of dropping 20-35 cm on camera.
	float RestHeight = 30.f;
	if (Cast<ASquadController>(Unit) == nullptr)
	{
		FHitResult HereHit;
		const FVector Here = Unit->GetActorLocation();
		if (GetWorld()->LineTraceSingleByChannel(HereHit, Here + FVector(0.f, 0.f, 500.f), Here - FVector(0.f, 0.f, 2000.f), ECC_WorldStatic, Params))
		{
			RestHeight = FMath::Clamp(Here.Z - HereHit.ImpactPoint.Z, 0.f, 120.f);
		}
	}
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		Location.Z = Hit.ImpactPoint.Z + RestHeight;
	}
	// TeleportPhysics: with ResetPhysics the Chaos vehicle body stayed in the pen and the actor snapped back to it
	// every tick (drift reset on every frame, physics 150 m away from the rendered hull).
	Unit->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	if (UPrimitiveComponent* RootBody = Cast<UPrimitiveComponent>(Unit->GetRootComponent()))
	{
		// No velocity carried over from the pen: a KV-2 dropped there kept its fall speed and bounced off the stage
		// several metres up, out of the side-profile strip.
		if (RootBody->IsSimulatingPhysics())
		{
			RootBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
			RootBody->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}
	UE_LOG(LogRTSShorts, Verbose, TEXT("RTS_SHORTS_PLACED wanted=%s got=%s"), *Location.ToCompactString(), *Unit->GetActorLocation().ToCompactString());
	if (Segment.TurretRotationSpeed > 0.f)
	{
		// Short slots: the turret must be on target well inside the first second.
		for (ACPPTurretsMaster* Turret : GetUnitTurrets(Unit))
		{
			if (IsValid(Turret))
			{
				Turret->SetTurretRotationSpeed(Segment.TurretRotationSpeed);
			}
		}
	}
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Unit->GetRootComponent()))
	{
		Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	PlaceSquadMembers(Unit, Location, Rotation);
}

void ARTSShortsDirector::PlaceSquadMembers(AActor* Unit, const FVector& Location, const FRotator& Rotation) const
{
	// Squad controllers are invisible; their soldiers (and a team weapon's gun) are separate actors that stay
	// behind unless moved here. Gun on the mark, crew in a loose arc behind it; infantry in a staggered line.
	ASquadController* Squad = Cast<ASquadController>(Unit);
	if (Squad == nullptr)
	{
		return;
	}
	const FVector Forward = Rotation.Vector();
	const FVector Right = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);
	AActor* TeamWeapon = nullptr;
	const int32 SegmentIndex = M_Units.IndexOfByPredicate([Unit](const TObjectPtr<AActor>& Entry) { return Entry.Get() == Unit; });
	if (M_SegmentTeamWeapons.IsValidIndex(SegmentIndex))
	{
		TeamWeapon = M_SegmentTeamWeapons[SegmentIndex].Get();
	}
	if (IsValid(TeamWeapon))
	{
		TeamWeapon->SetActorLocationAndRotation(Location + FVector(0.f, 0.f, 20.f), Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	TArray<ASquadUnit*> Soldiers = Squad->GetSquadUnitsChecked();
	Soldiers.RemoveAll([](const ASquadUnit* Soldier) { return not IsValid(Soldier); });
	const int32 Count = Soldiers.Num();
	const bool bHasSegment = M_Job.Segments.IsValidIndex(SegmentIndex);
	const int32 MaxVisible = bHasSegment ? M_Job.Segments[SegmentIndex].MaxSquadVisible : 0;
	const float CrewBack = bHasSegment ? M_Job.Segments[SegmentIndex].CrewBackCm : 280.f;
	const int32 Visible = MaxVisible > 0 ? FMath::Min(MaxVisible, Count) : Count;
	if (Visible < Count)
	{
		// The visible members are the ones carrying the captioned weapon: the segment's preferred weapon first,
		// launchers and grenade throwers last.
		const FString Preferred = bHasSegment ? M_Job.Segments[SegmentIndex].PreferredWeapon : FString();
		Soldiers.StableSort([&Preferred](const ASquadUnit& A, const ASquadUnit& B)
		{
			auto Rank = [&Preferred](const ASquadUnit& Soldier)
			{
				const AInfantryWeaponMaster* Weapon = const_cast<ASquadUnit&>(Soldier).GetInfantryWeapon();
				const FString Name = IsValid(Weapon) ? Weapon->GetClass()->GetName() : FString();
				if (not Preferred.IsEmpty() && Name.Contains(Preferred))
				{
					return -1;
				}
				const bool bLauncher = Name.Contains(TEXT("Faust")) || Name.Contains(TEXT("Schreck")) || Name.Contains(TEXT("Shrek")) || Name.Contains(TEXT("Bazooka"))
					|| Name.Contains(TEXT("Grenade")) || Name.Contains(TEXT("Launcher")) || Name.Contains(TEXT("PTRS"))
					|| Name.Contains(TEXT("Flame")) || Name.Contains(TEXT("Medic"));
				return bLauncher ? 1 : 0;
			};
			return Rank(A) < Rank(B);
		});
	}
	if (M_HiddenSoldiers.Num() < M_Job.Segments.Num())
	{
		M_HiddenSoldiers.SetNum(M_Job.Segments.Num());
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		ASquadUnit* Soldier = Soldiers[Index];
		const AInfantryWeaponMaster* Weapon = Soldier->GetInfantryWeapon();
		UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_SQUAD_MEMBER segment=%d index=%d weapon=%s visible=%d"), SegmentIndex, Index,
		       IsValid(Weapon) ? *Weapon->GetClass()->GetName() : TEXT("none"), Index < Visible ? 1 : 0);
		if (Index >= Visible)
		{
			// Hidden but close: the squad controller tracks its members' centroid, so parking them far away
			// drags the controller (and the drift lock) off the mark. 2.5 m behind the leader, invisible.
			const FVector Parked = Location - Forward * 250.f + Right * ((Index - Visible) * 90.f - 90.f) + FVector(0.f, 0.f, 5.f);
			Soldier->SetActorLocationAndRotation(Parked, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
			Soldier->SetActorHiddenInGame(true);
			if (bHasSegment)
			{
				M_HiddenSoldiers[SegmentIndex].AddUnique(Soldier);
			}
			continue;
		}
		Soldier->SetActorHiddenInGame(false);
		const float Lateral = (Index - (Visible - 1) * 0.5f) * (IsValid(TeamWeapon) ? 120.f : 170.f);
		const float Back = IsValid(TeamWeapon) ? CrewBack + 40.f * (Index % 2) : 90.f * (Index % 2);
		const FVector Spot = Location + Right * Lateral - Forward * Back + FVector(0.f, 0.f, 5.f);
		Soldier->SetActorLocationAndRotation(Spot, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ARTSShortsDirector::LimitStageEffects(AActor* Unit, const FRTSShortsSegment& Segment, const int32 Frame)
{
	const bool bCap = Segment.EffectMaxSeconds >= 0.f;
	const bool bScale = not FMath::IsNearlyEqual(Segment.EffectScale, 1.f);
	const bool bBoost = Segment.EffectBoostNames.Num() > 0 && not FMath::IsNearlyEqual(Segment.EffectBoostScale, 1.f);
	if (not bCap && not bScale && not bBoost && not Segment.bSuppressExhaust)
	{
		return;
	}
	const int32 MaxFrames = FMath::RoundToInt(FMath::Max(0.f, Segment.EffectMaxSeconds) * M_Job.Fps);
	const int32 FadeFrames = FMath::Max(1, FMath::RoundToInt(FMath::Max(0.1f, Segment.EffectFadeSeconds) * M_Job.Fps));
	for (TObjectIterator<UFXSystemComponent> It; It; ++It)
	{
		UFXSystemComponent* Component = *It;
		if (Component->GetWorld() != GetWorld())
		{
			continue;
		}
		const AActor* ComponentOwner = Component->GetOwner();
		// Level-placed actors (burning wrecks, ambient effects) keep running.
		if (ComponentOwner != nullptr && ComponentOwner->IsNetStartupActor())
		{
			continue;
		}
		const int32* FirstSeen = M_EffectFirstSeen.Find(Component);
		if (not Component->IsActive() && FirstSeen == nullptr)
		{
			continue;
		}
		const bool bExhaust = Component->GetName().Contains(TEXT("xhaust"))
			|| (Component->GetFXSystemAsset() != nullptr && Component->GetFXSystemAsset()->GetName().Contains(TEXT("xhaust")));
		if (bExhaust)
		{
			if (Segment.bSuppressExhaust && Component->IsActive())
			{
				Component->DeactivateImmediate();
			}
			continue;
		}
		if (bBoost && Component->IsActive())
		{
			BoostEffectScale(Component, Segment);
		}
		if (FirstSeen == nullptr)
		{
			// New effect: world-spawned ones (dust, smoke) get the segment's scale; the unit's own (muzzle flash
			// on the turret socket) keep theirs.
			const bool bOnUnit = ComponentOwner != nullptr && (ComponentOwner == Unit || ComponentOwner->IsAttachedTo(Unit));
			const bool bExempt = GetIsEffectExemptFromLimits(Segment, Frame);
			if (bScale && not bOnUnit && not bExempt)
			{
				Component->SetWorldScale3D(Component->GetComponentScale() * Segment.EffectScale);
			}
			if (bOnUnit && not FMath::IsNearlyEqual(Segment.EffectUnitScale, 1.f))
			{
				// The unit's own effects (muzzle flash and smoke on the barrel socket), e.g. a bigger fireball
				// for a far side-profile camera.
				Component->SetWorldScale3D(Component->GetComponentScale() * Segment.EffectUnitScale);
			}
			M_EffectFirstSeen.Add(Component, Frame);
			M_EffectBaseScale.Add(Component, Component->GetComponentScale());
			const FString AssetName = Component->GetFXSystemAsset() ? Component->GetFXSystemAsset()->GetName() : FString(TEXT("none"));
			UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_FX_NEW frame=%d asset=%s owner=%s onUnit=%d dist=%.0f"), Frame, *AssetName,
			       ComponentOwner ? *ComponentOwner->GetName() : TEXT("none"), bOnUnit ? 1 : 0,
			       FVector::Dist(Component->GetComponentLocation(), Unit->GetActorLocation()));
			if (not bExempt && Segment.EffectKillNames.ContainsByPredicate([&AssetName](const FString& Part) { return AssetName.Contains(Part); }))
			{
				// Named effects (e.g. the recoil ground dust that hides the hull) are removed outright, before any smoke
				// split: a coax MG firing every 6 frames otherwise left two scaled smoke copies per shot (a grey veil).
				Component->DeactivateImmediate();
				M_EffectFirstSeen.Remove(Component);
				M_EffectBaseScale.Remove(Component);
				continue;
			}
			if (UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Component))
			{
				if (const UNiagaraSystem* System = Niagara->GetAsset())
				{
					FString Emitters;
					TArray<FName> Disabled;
					for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
					{
						const FString EmitterName = Handle.GetName().ToString();
						Emitters += EmitterName + TEXT(",");
						if (Segment.EffectDisableEmitters.ContainsByPredicate([&EmitterName](const FString& Part) { return EmitterName.Contains(Part); }))
						{
							Disabled.Add(Handle.GetName());
						}
					}
					const bool bSplitSmoke = bOnUnit && not bExempt && Segment.EffectUnitSmokeScale > 0.f;
					TArray<FName> SmokeEmitters;
					if (bSplitSmoke)
					{
						for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
						{
							const FName HandleName = Handle.GetName();
							if (HandleName.ToString().Contains(TEXT("smoke")) && not Disabled.Contains(HandleName))
							{
								SmokeEmitters.Add(HandleName);
							}
						}
					}
					if (SmokeEmitters.Num() > 0)
					{
						// Flash and smoke need different sizes (a far side-profile camera wants a big fireball but
						// not a sky-filling cloud): this component keeps the flash, a copy on the same socket plays
						// only the smoke at EffectUnitSmokeScale.
						const float UnitScale = FMath::IsNearlyZero(Segment.EffectUnitScale) ? 1.f : Segment.EffectUnitScale;
						const FVector OriginalScale = Niagara->GetComponentScale() / UnitScale;
						// No opacity parameter is exposed: density comes from stacking copies. The first plays out in front
						// of the muzzle (EffectSmokeForwardOffset), the others right at it so there is no gap after the shot.
						const int32 CopyCount = FMath::Clamp(Segment.EffectSmokeCopies, 1, 4);
						for (int32 CopyIndex = 0; CopyIndex < CopyCount; ++CopyIndex)
						{
							UNiagaraComponent* SmokeCopy = UNiagaraFunctionLibrary::SpawnSystemAttached(
								const_cast<UNiagaraSystem*>(System), Niagara->GetAttachParent(), Niagara->GetAttachSocketName(),
								Niagara->GetRelativeLocation(), Niagara->GetRelativeRotation(), EAttachLocation::KeepRelativeOffset, true);
							if (not IsValid(SmokeCopy))
							{
								continue;
							}
							const float CopyScale = CopyIndex == 0 ? 1.f : FMath::Max(0.05f, Segment.EffectSmokeExtraCopyScale);
							SmokeCopy->SetWorldScale3D(OriginalScale * Segment.EffectUnitSmokeScale * CopyScale);
							for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
							{
								if (not SmokeEmitters.Contains(Handle.GetName()))
								{
									SmokeCopy->SetEmitterEnable(Handle.GetName(), false);
								}
							}
							if (CopyIndex == 0 && not FMath::IsNearlyZero(Segment.EffectSmokeForwardOffset))
							{
								// Out in front of the muzzle, so the cloud does not sit over the barrel.
								SmokeCopy->AddWorldOffset(SmokeCopy->GetForwardVector() * Segment.EffectSmokeForwardOffset);
							}
							if (not FMath::IsNearlyZero(Segment.EffectSmokeAwayOffset) && IsValid(M_Camera))
							{
								// Behind the barrel's plane as seen from the camera: the barrel stays readable in front of
								// the cloud instead of sinking into it (the hull plates are nearer the camera anyway).
								FVector Away = SmokeCopy->GetComponentLocation() - M_Camera->GetActorLocation();
								Away.Z = 0.f;
								SmokeCopy->AddWorldOffset(Away.GetSafeNormal() * Segment.EffectSmokeAwayOffset);
							}
							M_EffectFirstSeen.Add(SmokeCopy, Frame);
							M_EffectBaseScale.Add(SmokeCopy, SmokeCopy->GetComponentScale());
							M_SmokeCopies.Add(SmokeCopy);
						}
						Disabled.Append(SmokeEmitters);
					}
					if (Disabled.Num() > 0 && not bExempt)
					{
						// The spawn burst has already simulated by the time the director sees the component:
						// restart the system (clears those particles) and disable the emitters before its next spawn.
						Niagara->ReinitializeSystem();
						for (const FName& EmitterName : Disabled)
						{
							Niagara->SetEmitterEnable(EmitterName, false);
						}
					}
					UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_FX_EMITTERS asset=%s emitters=%s disabled=%d"), *AssetName, *Emitters, Disabled.Num());
				}
			}
			continue;
		}
		if (not bCap || GetIsEffectExemptFromLimits(Segment, *FirstSeen))
		{
			continue;
		}
		// Stop spawning at EffectMaxSeconds, then shrink what is left to nothing over 0.6 s (a fade, not a pop).
		const int32 Age = Frame - *FirstSeen;
		// The smoke copy of a split effect has its own hold and fade, and fades at normal speed (smoke drifts out).
		const bool bSmokeCopy = M_SmokeCopies.Contains(Component);
		const int32 CompMaxFrames = bSmokeCopy && Segment.EffectSmokeMaxSeconds >= 0.f
			? FMath::RoundToInt(Segment.EffectSmokeMaxSeconds * M_Job.Fps) : MaxFrames;
		const int32 CompFadeFrames = bSmokeCopy && Segment.EffectSmokeFadeSeconds > 0.f
			? FMath::Max(1, FMath::RoundToInt(Segment.EffectSmokeFadeSeconds * M_Job.Fps)) : FadeFrames;
		const float CompFadeSpeed = bSmokeCopy ? 1.f : Segment.EffectFadeSpeed;
		if (Age <= CompMaxFrames)
		{
			continue;
		}
		if (Component->IsActive() && M_EffectStopped.Contains(Component))
		{
			// The weapon re-fired a pooled component this director had already stopped: a new shot, so it gets
			// its own EffectMaxSeconds instead of being cut on its first frame.
			M_EffectStopped.Remove(Component);
			M_EffectFirstSeen.Add(Component, Frame);
			if (UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Component))
			{
				Niagara->SetCustomTimeDilation(1.f);
			}
			continue;
		}
		if (Component->IsActive())
		{
			Component->Deactivate();
			M_EffectStopped.Add(Component);
		}
		if (UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Component))
		{
			// Age the remaining particles faster so they fade out on their own curve instead of being cut.
			if (CompFadeSpeed > 1.f && not FMath::IsNearlyEqual(Niagara->GetCustomTimeDilation(), CompFadeSpeed))
			{
				Niagara->SetCustomTimeDilation(CompFadeSpeed);
			}
		}
		const float Alpha = FMath::Clamp(static_cast<float>(Age - CompMaxFrames) / CompFadeFrames, 0.f, 1.f);
		if (Alpha >= 1.f)
		{
			if (UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Component))
			{
				Niagara->SetCustomTimeDilation(1.f);
			}
			Component->DeactivateImmediate();
			if (const FVector* Base = M_EffectBaseScale.Find(Component))
			{
				// Pooled components come back later: give them their original size again.
				Component->SetWorldScale3D(*Base);
			}
			M_EffectFirstSeen.Remove(Component);
			M_EffectBaseScale.Remove(Component);
			M_EffectStopped.Remove(Component);
			M_SmokeCopies.Remove(Component);
		}
	}
}

void ARTSShortsDirector::BoostEffectScale(UFXSystemComponent* Component, const FRTSShortsSegment& Segment)
{
	const UFXSystemAsset* Asset = Component->GetFXSystemAsset();
	if (Asset == nullptr)
	{
		return;
	}
	const FString AssetName = Asset->GetName();
	if (not Segment.EffectBoostNames.ContainsByPredicate([&AssetName](const FString& Part) { return AssetName.Contains(Part); }))
	{
		return;
	}
	// A pooled effect gets its weapon's scale back on every play: boost whatever scale it has now unless it is still
	// the one this director gave it.
	const FVector Current = Component->GetComponentScale();
	const FVector* LastBoosted = M_EffectBoostedScale.Find(Component);
	constexpr float SameScaleTolerance = 0.01f;
	if (LastBoosted != nullptr && Current.Equals(*LastBoosted, SameScaleTolerance))
	{
		return;
	}
	const FVector Boosted = Current * Segment.EffectBoostScale;
	Component->SetWorldScale3D(Boosted);
	M_EffectBoostedScale.Add(Component, Boosted);
}

bool ARTSShortsDirector::GetIsEffectExemptFromLimits(const FRTSShortsSegment& Segment, const int32 FirstSeenFrame) const
{
	if (Segment.EffectLimitUntilSeconds < 0.f || not M_SegmentStartFrames.IsValidIndex(M_CurrentSegment))
	{
		return false;
	}
	const int32 LimitEndFrame = M_SegmentStartFrames[M_CurrentSegment] + FMath::RoundToInt(Segment.EffectLimitUntilSeconds * M_Job.Fps);
	return FirstSeenFrame >= LimitEndFrame;
}

void ARTSShortsDirector::LogVisibleSoldierShots(const int32 Frame)
{
	// One line per round the framed soldier fires (its magazine count drops): cut leads are set from these frames.
	const ASquadUnit* Soldier = M_Units.IsValidIndex(M_CurrentSegment)
		? Cast<ASquadUnit>(GetFramingSubject(M_Units[M_CurrentSegment].Get())) : nullptr;
	AInfantryWeaponMaster* Weapon = IsValid(Soldier) ? Soldier->GetInfantryWeapon() : nullptr;
	if (not IsValid(Weapon))
	{
		return;
	}
	int32 Rounds = 0;
	for (const UWeaponState* State : Weapon->GetWeapons())
	{
		Rounds += IsValid(State) ? State->GetCurrentMagCapacity() : 0;
	}
	if (M_LastVisibleRounds >= 0 && Rounds < M_LastVisibleRounds)
	{
		UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_SHOT segment=%d frame=%d local=%d rounds=%d"), M_CurrentSegment, Frame,
		       Frame - M_SegmentStartFrames[M_CurrentSegment], Rounds);
	}
	M_LastVisibleRounds = Rounds;
}

void ARTSShortsDirector::ApplyInfantryFireInterval(AActor* Unit, const FRTSShortsSegment& Segment) const
{
	ASquadController* Squad = Cast<ASquadController>(Unit);
	if (Segment.InfantryFireIntervalSeconds < 0.f || not IsValid(Squad))
	{
		return;
	}
	int32 Changed = 0;
	for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
	{
		AInfantryWeaponMaster* Weapon = IsValid(Soldier) ? Soldier->GetInfantryWeapon() : nullptr;
		if (not IsValid(Weapon))
		{
			continue;
		}
		for (UWeaponState* State : Weapon->GetWeapons())
		{
			FWeaponData* Data = IsValid(State) ? State->GetWeaponDataToUpgrade() : nullptr;
			if (Data == nullptr)
			{
				continue;
			}
			Data->BaseCooldown = FMath::Min(Data->BaseCooldown, Segment.InfantryFireIntervalSeconds);
			Data->CooldownFlux = 0;
			// A faster bolt rifle emptied its 5-round clip before the cut and reloaded on camera: a deep magazine,
			// filled now, keeps it firing through the slot.
			constexpr int32 ShortsMagazine = 30;
			Data->MagCapacity = FMath::Max(Data->MagCapacity, ShortsMagazine);
			State->ForceInstantReload();
			++Changed;
		}
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_FIRE_INTERVAL squad=%s weapons=%d interval=%.2f"), *Squad->GetName(), Changed,
	       Segment.InfantryFireIntervalSeconds);
}

void ARTSShortsDirector::DisarmSegmentTarget() const
{
	// The dummy target stays a valid target but never fires back: its turrets and its soldiers' weapons stop
	// ticking (an AI T-70 killed the bazooka man, a Panzerfaust squad blew up the PTRS crew). Every tick, because
	// squad members spawn a little after their controller.
	AActor* Target = M_SegmentTarget.Get();
	if (not IsValid(Target))
	{
		return;
	}
	for (ACPPTurretsMaster* Turret : GetUnitTurrets(Target))
	{
		if (IsValid(Turret) && Turret->IsActorTickEnabled())
		{
			Turret->SetActorTickEnabled(false);
		}
	}
	if (ASquadController* Squad = Cast<ASquadController>(Target))
	{
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			AInfantryWeaponMaster* Weapon = IsValid(Soldier) ? Soldier->GetInfantryWeapon() : nullptr;
			if (IsValid(Weapon) && Weapon->IsActorTickEnabled())
			{
				Weapon->SetActorTickEnabled(false);
			}
		}
	}
}

void ARTSShortsDirector::SuppressUnitDecals(AActor* Unit, const int32 SegmentIndex) const
{
	// Infantry must not receive projected decals (ground ruts, craters, dirt): the soldiers' skeletons, every mesh
	// of the weapon they carry (and anything else attached to them) and a crewed team weapon's gun. Vehicles and
	// selection decals are left alone.
	if (not M_Job.bSuppressUnitDecals)
	{
		return;
	}
	ASquadController* Squad = Cast<ASquadController>(Unit);
	if (Squad == nullptr)
	{
		return;
	}
	auto SuppressOn = [](const AActor* Actor)
	{
		if (not IsValid(Actor))
		{
			return;
		}
		TInlineComponentArray<UMeshComponent*> Meshes(Actor);
		for (UMeshComponent* Mesh : Meshes)
		{
			if (IsValid(Mesh) && Mesh->bReceivesDecals)
			{
				Mesh->SetReceivesDecals(false);
			}
		}
	};
	for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
	{
		if (not IsValid(Soldier))
		{
			continue;
		}
		SuppressOn(Soldier);
		SuppressOn(Soldier->GetInfantryWeapon());
		TArray<AActor*> Attached;
		Soldier->GetAttachedActors(Attached, true, true);
		for (const AActor* Child : Attached)
		{
			SuppressOn(Child);
		}
	}
	if (M_SegmentTeamWeapons.IsValidIndex(SegmentIndex))
	{
		SuppressOn(M_SegmentTeamWeapons[SegmentIndex].Get());
	}
}

const AActor* ARTSShortsDirector::GetFramingSubject(const AActor* Unit) const
{
	// GetSquadUnitsChecked is not const; the squad is only read here.
	ASquadController* Squad = const_cast<ASquadController*>(Cast<ASquadController>(Unit));
	if (Squad == nullptr)
	{
		return Unit;
	}
	const int32 SegmentIndex = M_Units.IndexOfByPredicate([Unit](const TObjectPtr<AActor>& Entry) { return Entry.Get() == Unit; });
	if (M_SegmentTeamWeapons.IsValidIndex(SegmentIndex) && IsValid(M_SegmentTeamWeapons[SegmentIndex].Get()))
	{
		return M_SegmentTeamWeapons[SegmentIndex].Get();
	}
	const AActor* FirstValid = nullptr;
	for (const ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
	{
		if (not IsValid(Soldier))
		{
			continue;
		}
		if (not Soldier->IsHidden())
		{
			return Soldier;
		}
		FirstValid = FirstValid ? FirstValid : Soldier;
	}
	return FirstValid ? FirstValid : Unit;
}

void ARTSShortsDirector::ParkUnit(AActor* Unit, const int32 SegmentIndex) const
{
	const FTransform Pen = GetPenTransform(SegmentIndex);
	Unit->SetActorLocationAndRotation(Pen.GetLocation() + FVector(0.f, 0.f, 50.f), Pen.Rotator(), false, nullptr,
	                                  ETeleportType::TeleportPhysics);
	PlaceSquadMembers(Unit, Pen.GetLocation(), Pen.Rotator());
}

void ARTSShortsDirector::FrameCamera(const AActor* Unit, const FRTSShortsSegment& Segment,
                                     const FVector& TargetWorld) const
{
	if (not GetIsValidCamera())
	{
		return;
	}
	const FRTSShortsCameraParams& Params = Segment.Camera;
	// Squads: frame the gun (team weapons) or the leader soldier, never the invisible controller whose bounds
	// span the whole squad.
	const AActor* Subject = GetFramingSubject(Unit);
	FVector Origin;
	FVector Extent;
	Subject->GetActorBounds(false, Origin, Extent);
	const FRotator UnitRotation(0.f, M_Stage->GetActorRotation().Yaw + Segment.UnitYawOffset, 0.f);
	const FVector Forward = UnitRotation.Vector();
	const FVector Right = FRotationMatrix(UnitRotation).GetUnitAxis(EAxis::Y);
	// The unit was just teleported; use the placement location and only the size of its bounds. A team weapon's
	// actor origin can sit far from its visible gun (ZiS-3), so squads frame on the bounds centre instead.
	const FVector Centre = Subject != Unit ? Origin : Subject->GetActorLocation();
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_SUBJECT unit=%s subject=%s unitLoc=%s subjectLoc=%s boundsCentre=%s extent=%s"),
	       *Unit->GetName(), *Subject->GetName(), *Unit->GetActorLocation().ToCompactString(), *Subject->GetActorLocation().ToCompactString(),
	       *Origin.ToCompactString(), *Extent.ToCompactString());
	Origin = FVector(Centre.X, Centre.Y, Unit->GetActorLocation().Z + Extent.Z);
	const float GroundZ = Unit->GetActorLocation().Z;
	float BackDistance = Params.BackDistance;
	float CameraHeight = Params.Height;
	float LookAtHeight = Params.LookAtHeight;
	if (Params.FitWidthFraction > 0.f)
	{
		// Front silhouette width from the bounds (the unit faces the stage's forward axis); 13.5 mm sensor width.
		UCineCameraComponent* Cine = M_Camera->GetCineCameraComponent();
		const float SensorWidth = Cine ? Cine->Filmback.SensorWidth : 13.5f;
		const float HalfFov = FMath::Atan(SensorWidth * 0.5f / FMath::Max(1.f, Params.FocalLength));
		const float Width = 2.f * FMath::Max(Extent.Y, 60.f);
		const float Distance = FMath::Max(Params.MinDistance, Width / (2.f * Params.FitWidthFraction * FMath::Tan(HalfFov)));
		BackDistance = (Params.BackDistance < 0.f ? -1.f : 1.f) * Distance;
		if (Params.HeightPerDistance > 0.f)
		{
			CameraHeight = FMath::Max(Params.Height, Params.HeightPerDistance * Distance);
		}
		if (Params.LookAtPerDistance > 0.f)
		{
			LookAtHeight = Params.LookAtPerDistance * Distance;
		}
		UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_FIT width=%.0f distance=%.0f focal=%.0f height=%.0f lookAt=%.0f"), Width, Distance, Params.FocalLength, CameraHeight, LookAtHeight);
	}
	FVector CameraLocation = Origin - Forward * BackDistance + Right * Params.SideOffset;
	if (Params.FitWidthFraction > 0.f && Params.FitPitch > 0.f)
	{
		// Lay the fitted distance out along a pitched ray: horizontal = d cos(pitch), vertical = d sin(pitch),
		// the horizontal leg rotated FitAzimuth degrees off the unit's front (or back) axis.
		const float Distance = FMath::Abs(BackDistance);
		const float Horizontal = Distance * FMath::Cos(FMath::DegreesToRadians(Params.FitPitch));
		const float Sign = Params.BackDistance < 0.f ? -1.f : 1.f;
		const FVector Direction = (-Forward * Sign).RotateAngleAxis(Params.FitAzimuth, FVector::UpVector);
		CameraLocation = Origin + Direction * Horizontal;
		CameraHeight = LookAtHeight + Distance * FMath::Sin(FMath::DegreesToRadians(Params.FitPitch));
	}
	CameraLocation.Z = GroundZ + CameraHeight;
	FVector LookAt = FMath::Lerp(Origin, TargetWorld, Params.LookAtBlend) + Forward * Params.LookAtForward;
	LookAt.Z = GroundZ + LookAtHeight;
	FRotator Rotation = (LookAt - CameraLocation).Rotation();
	Rotation.Pitch += Params.ExtraPitch;
	Rotation.Roll = 0.f;
	M_Camera->SetActorLocationAndRotation(CameraLocation, Rotation);
	M_Orbit = FRTSShortsOrbitState();
	if (not FMath::IsNearlyZero(Params.OrbitDegrees))
	{
		M_Orbit.bIsActive = true;
		M_Orbit.Pivot = LookAt;
		M_Orbit.StartOffset = CameraLocation - LookAt;
		M_Orbit.Degrees = Params.OrbitDegrees;
		M_Orbit.StartSeconds = Params.OrbitStartSeconds;
		M_Orbit.DurationSeconds = FMath::Max(0.1f, Params.OrbitSeconds);
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_CAMERA origin=%s camera=%s lookAt=%s rot=%s focal=%.0f"), *Origin.ToCompactString(),
	       *CameraLocation.ToCompactString(), *LookAt.ToCompactString(), *Rotation.ToCompactString(), Params.FocalLength);
	if (UCineCameraComponent* CineComponent = M_Camera->GetCineCameraComponent())
	{
		CineComponent->SetCurrentFocalLength(Params.FocalLength);
		// No depth of field: the default manual focus distance blurred small units framed a few metres away.
		CineComponent->FocusSettings.FocusMethod = ECameraFocusMethod::Disable;
		CineComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
		CineComponent->PostProcessSettings.AutoExposureBias = Params.ExposureBias;
		// No motion blur: at 60 fps it smeared rifles into unreadable streaks.
		CineComponent->PostProcessSettings.bOverride_MotionBlurAmount = true;
		CineComponent->PostProcessSettings.MotionBlurAmount = 0.f;
		CineComponent->PostProcessSettings.bOverride_ColorSaturation = true;
		CineComponent->PostProcessSettings.ColorSaturation = FVector4(Params.ColorSaturation, Params.ColorSaturation, Params.ColorSaturation, 1.f);
		CineComponent->PostProcessSettings.bOverride_WhiteTemp = not FMath::IsNearlyEqual(Params.WhiteTemp, 6500.f);
		CineComponent->PostProcessSettings.WhiteTemp = Params.WhiteTemp;
		CineComponent->PostProcessBlendWeight = 1.f;
	}
	ApplyCameraCut(CameraLocation);
}

void ARTSShortsDirector::UpdateOrbitCamera(const float SegmentSeconds) const
{
	if (not M_Orbit.bIsActive || not GetIsValidCamera())
	{
		return;
	}
	const float Alpha = FMath::Clamp((SegmentSeconds - M_Orbit.StartSeconds) / M_Orbit.DurationSeconds, 0.f, 1.f);
	// Smoothstep: the pan eases out of the front view and settles on the rear view instead of stopping hard.
	const float Eased = Alpha * Alpha * (3.f - 2.f * Alpha);
	const FVector Offset = M_Orbit.StartOffset.RotateAngleAxis(M_Orbit.Degrees * Eased, FVector::UpVector);
	const FVector CameraLocation = M_Orbit.Pivot + Offset;
	FRotator Rotation = (M_Orbit.Pivot - CameraLocation).Rotation();
	Rotation.Roll = 0.f;
	M_Camera->SetActorLocationAndRotation(CameraLocation, Rotation);
}

float ARTSShortsDirector::ReadLargestWeaponRange(const AActor* Unit) const
{
	float Largest = 0.f;
	for (const ACPPTurretsMaster* Turret : GetUnitTurrets(Unit))
	{
		const UWeaponState* Weapon = IsValid(Turret) ? Turret->GetLargestCalibreWeapon() : nullptr;
		if (IsValid(Weapon))
		{
			Largest = FMath::Max(Largest, Weapon->GetRange());
		}
	}
	return Largest;
}

FVector ARTSShortsDirector::GetTargetWorldLocation(const FRTSShortsSegment& Segment, const AActor* Unit) const
{
	const FTransform StageTransform = M_Stage->GetActorTransform();
	FVector Offset = Segment.TargetOffset;
	// Keep the impact well inside the weapon's range, else the tank drives instead of firing.
	const float Range = ReadLargestWeaponRange(Unit);
	const float MaxDistance = Range > 0.f ? Range * RTSShortsDirectorPrivate::RangeUseFraction : 0.f;
	if (MaxDistance > 0.f && Offset.Size2D() > MaxDistance)
	{
		Offset = Offset.GetSafeNormal2D() * MaxDistance;
	}
	FVector Target = StageTransform.TransformPosition(Offset);
	FHitResult Hit;
	const FVector Start = Target + FVector(0.f, 0.f, 3000.f);
	const FVector End = Target - FVector(0.f, 0.f, 3000.f);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic))
	{
		Target.Z = Hit.ImpactPoint.Z;
	}
	return Target;
}

void ARTSShortsDirector::OrderAttackGround(const int32 SegmentIndex)
{
	bM_FireOrdered = true;
	if (M_Battle.bIsActive)
	{
		// A battle fights unit against unit: every tank gets the nearest opponent instead of a ground point.
		IssueBattleOrders();
		M_Manifest.Segments[SegmentIndex].FireOrderFrame = GetTimelineFrame();
		return;
	}
	AActor* Unit = M_Units[SegmentIndex].Get();
	ICommands* Commands = Cast<ICommands>(Unit);
	if (Commands == nullptr)
	{
		MarkSegmentSkipped(SegmentIndex, TEXT("unit has no commands interface"));
		return;
	}
	const FRTSShortsSegment& Segment = M_Job.Segments[SegmentIndex];
	FVector Target = GetTargetWorldLocation(Segment, Unit);
	const bool bTargetAsGround = Segment.bAttackTargetAsGround && M_SegmentTarget.IsValid();
	if (bTargetAsGround)
	{
		// A building is no unit: fire at a point on it (the ground trace would land on its roof).
		Target = M_SegmentTarget->GetActorLocation() + FVector(0.f, 0.f, Segment.TargetAimHeight);
	}
	const ECommandQueueError Result = M_SegmentTarget.IsValid() && not bTargetAsGround
		? Commands->AttackActor(M_SegmentTarget.Get(), true)
		: Commands->AttackGround(Target, true);
	M_Manifest.Segments[SegmentIndex].FireOrderFrame = GetTimelineFrame();
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_ATTACK_GROUND index=%d frame=%d result=%d target=%s"),
	       SegmentIndex, M_Manifest.Segments[SegmentIndex].FireOrderFrame, static_cast<int32>(Result), *Target.ToString());
}

void ARTSShortsDirector::OrderStop(const int32 SegmentIndex)
{
	bM_StopOrdered = true;
	if (M_Battle.bIsActive)
	{
		// A battle plays until one side is gone; the slot end (or the decision) ends it, not a stop order.
		return;
	}
	ICommands* Commands = Cast<ICommands>(M_Units[SegmentIndex].Get());
	if (Commands == nullptr)
	{
		return;
	}
	Commands->StopCommand();
	M_Manifest.Segments[SegmentIndex].StopOrderFrame = GetTimelineFrame();
}

int32 ARTSShortsDirector::ReadLargestCaliberMm(const AActor* Unit) const
{
	int32 Largest = 0;
	for (const ACPPTurretsMaster* Turret : GetUnitTurrets(Unit))
	{
		const UWeaponState* Weapon = IsValid(Turret) ? Turret->GetLargestCalibreWeapon() : nullptr;
		if (IsValid(Weapon))
		{
			Largest = FMath::Max(Largest, Weapon->GetRawWeaponData().WeaponCalibre);
		}
	}
	return Largest;
}

void ARTSShortsDirector::MarkSegmentSkipped(const int32 SegmentIndex, const FString& Reason)
{
	if (not M_Manifest.Segments.IsValidIndex(SegmentIndex))
	{
		return;
	}
	M_Manifest.Segments[SegmentIndex].bSkipped = true;
	M_Manifest.Segments[SegmentIndex].SkipReason = Reason;
	UE_LOG(LogRTSShorts, Warning, TEXT("RTS_SHORTS_SEGMENT_SKIPPED index=%d reason=%s"), SegmentIndex, *Reason);
}

void ARTSShortsDirector::NotifyRenderFinished()
{
	if (M_CurrentSegment >= 0 && M_CurrentSegment < M_Job.Segments.Num())
	{
		EndSegment(M_CurrentSegment);
	}
	M_Manifest.bComplete = M_CurrentSegment >= M_Job.Segments.Num() - 1;
	M_Phase = ERTSShortsDirectorPhase::Finished;
	WriteSubmixCaptureWav();
	WriteManifest();
}

void ARTSShortsDirector::RunAudioProbe(const int32 Local) const
{
	// Plays the Panzerjaeger launch cue (silent in renders so far) and the 20 mm cue (audible) through the different
	// playback paths at known frames, 2 m in front of the listener, so the capture WAV tells which path renders.
	static const TCHAR* PzJagerPath = TEXT("/Game/RTS_Survival/Sounds/WeaponSounds/SmallCaliber/Pzjager/S_FireWeapon_PZjager.S_FireWeapon_PZjager");
	static const TCHAR* AutoPath = TEXT("/Game/RTS_Survival/Sounds/WeaponSounds/SmallCaliber/20MMAuto/S_FireWeapon_20MM_Auto.S_FireWeapon_20MM_Auto");
	USoundBase* PzJager = LoadObject<USoundBase>(nullptr, PzJagerPath);
	USoundBase* Auto = LoadObject<USoundBase>(nullptr, AutoPath);
	if (PzJager == nullptr || Auto == nullptr || not GetIsValidCamera())
	{
		return;
	}
	const FVector At = M_Camera->GetActorLocation() + M_Camera->GetActorForwardVector() * 200.f;
	const TCHAR* Probe = nullptr;
	switch (Local)
	{
	case 30: UGameplayStatics::PlaySoundAtLocation(GetWorld(), PzJager, At); Probe = TEXT("PlaySoundAtLocation PzJager"); break;
	case 90: UGameplayStatics::PlaySound2D(GetWorld(), PzJager); Probe = TEXT("PlaySound2D PzJager"); break;
	case 150: UGameplayStatics::SpawnSoundAtLocation(GetWorld(), PzJager, At); Probe = TEXT("SpawnSoundAtLocation PzJager"); break;
	case 210: UGameplayStatics::PlaySoundAtLocation(GetWorld(), Auto, At); Probe = TEXT("PlaySoundAtLocation 20MM"); break;
	default: break;
	}
	if (Probe != nullptr)
	{
		UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_AUDIO_PROBE local=%d frame=%d what=%s"), Local, GetTimelineFrame(), Probe);
	}
}

void ARTSShortsDirector::ApplyStageVisibility() const
{
	// Actors tagged "ShortsOnly_<Stage>" belong to one stage's set (its sky, ground, props): show the ones of the
	// job's stage and hide every other stage's set, so several backdrops can share the level.
	const FString Prefix = TEXT("ShortsOnly_");
	const FString Wanted = Prefix + M_Job.StageName;
	int32 Shown = 0;
	int32 Hidden = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		bool bTagged = false;
		bool bForThisStage = false;
		for (const FName& Tag : Actor->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(Prefix))
			{
				bTagged = true;
				bForThisStage |= TagString.Equals(Wanted, ESearchCase::IgnoreCase);
			}
		}
		if (not bTagged)
		{
			continue;
		}
		Actor->SetActorHiddenInGame(not bForThisStage);
		Actor->SetActorEnableCollision(bForThisStage);
		bForThisStage ? ++Shown : ++Hidden;
	}
	// Directional lights are switched by name instead of tags: "<Stage>_Sun" lights only its stage, and the
	// plain "Sun" lights every stage that has no sun of its own.
	const FString StageSun = M_Job.StageName + TEXT("_Sun");
	bool bStageHasSun = false;
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		bStageHasSun |= It->GetActorNameOrLabel().Equals(StageSun, ESearchCase::IgnoreCase);
	}
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		const FString Name = It->GetActorNameOrLabel();
		bool bShow;
		if (Name.EndsWith(TEXT("_Sun"), ESearchCase::IgnoreCase))
		{
			bShow = Name.Equals(StageSun, ESearchCase::IgnoreCase);
		}
		else
		{
			bShow = not bStageHasSun;
		}
		It->SetActorHiddenInGame(not bShow);
		if (ULightComponent* Light = It->GetLightComponent())
		{
			Light->SetVisibility(bShow);
		}
		bShow ? ++Shown : ++Hidden;
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_STAGE_VISIBILITY stage=%s shown=%d hidden=%d stageSun=%d"), *M_Job.StageName, Shown, Hidden, bStageHasSun ? 1 : 0);
}

void ARTSShortsDirector::ForceSoundsNonSpatialized() const
{
	// The non-realtime audio platform (-deterministicaudio) reports a 7.1 channel array on its stereo stream, so
	// FMixerDevice pans every 3D source into channels that do not exist and they render as silence. 2D sources use
	// the stereo channel map and render fine, so every active sound is made non-spatialized before its first update.
	FAudioDevice* AudioDevice = GetWorld()->GetAudioDeviceRaw();
	if (AudioDevice == nullptr)
	{
		return;
	}
	// Attenuation assets are copied into each new active sound, so switching them to 2D here makes every sound
	// started afterwards non-spatialized regardless of how the active-sound flag is consumed.
	for (TObjectIterator<USoundAttenuation> It; It; ++It)
	{
		if (It->Attenuation.bSpatialize)
		{
			It->Attenuation.bSpatialize = false;
			UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_ATTENUATION_2D %s"), *It->GetName());
		}
	}
	FAudioThread::RunCommandOnAudioThread([AudioDevice]()
	{
		for (FActiveSound* ActiveSound : AudioDevice->GetActiveSounds())
		{
			if (ActiveSound != nullptr)
			{
				ActiveSound->bAllowSpatialization = false;
			}
		}
	});
}

void ARTSShortsDirector::LogActiveSounds(const int32 Frame) const
{
	FAudioDevice* AudioDevice = GetWorld()->GetAudioDeviceRaw();
	if (AudioDevice == nullptr)
	{
		UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_SOUNDS frame=%d no-audio-device"), Frame);
		return;
	}
	FTransform ListenerTransform;
	AudioDevice->GetListenerTransform(0, ListenerTransform);
	const FString Listener = ListenerTransform.GetLocation().ToCompactString();
	const bool bPaused = GetWorld()->IsPaused();
	const float GameDelta = GetWorld()->GetDeltaSeconds();
	const double Captured = M_AudioCapture.IsValid() ? M_AudioCapture->GetCapturedSeconds() : -1.0;
	// The active sound list belongs to the audio thread; read it there and log from there.
	FAudioThread::RunCommandOnAudioThread([AudioDevice, Frame, Listener, bPaused, GameDelta, Captured]()
	{
		FString Names;
		int32 Audible = 0;
		for (const FActiveSound* ActiveSound : AudioDevice->GetActiveSounds())
		{
			if (ActiveSound == nullptr || ActiveSound->GetSound() == nullptr)
			{
				continue;
			}
			float Actual = 0.f;
			FVector Location = FVector::ZeroVector;
			for (const TPair<UPTRINT, FWaveInstance*>& Pair : ActiveSound->GetWaveInstances())
			{
				if (Pair.Value != nullptr)
				{
					Actual = FMath::Max(Actual, Pair.Value->GetActualVolume());
					Location = Pair.Value->Location;
				}
			}
			Audible += Actual > 0.001f ? 1 : 0;
			FString Detail;
			for (const TPair<UPTRINT, FWaveInstance*>& Pair : ActiveSound->GetWaveInstances())
			{
				const FWaveInstance* Wave = Pair.Value;
				if (Wave == nullptr || Wave->WaveData == nullptr)
				{
					continue;
				}
				// Per wave: pitch, low/high-pass frequencies and distance, to find why an "audible" sound renders silence.
				Detail += FString::Printf(TEXT("[%s p=%.2f lpf=%.0f/%.0f/%.0f/%.0f hpf=%.0f d=%.0f paused=%d spat=%d]"),
				                          *Wave->WaveData->GetName(), Wave->Pitch, Wave->LowPassFilterFrequency,
				                          Wave->SoundClassFilterFrequency, Wave->OcclusionFilterFrequency,
				                          Wave->AttenuationLowpassFilterFrequency, Wave->AttenuationHighpassFilterFrequency,
				                          Wave->ListenerToSoundDistance, Wave->bIsPaused ? 1 : 0, Wave->SpatializationMethod != ESoundSpatializationAlgorithm::SPATIALIZATION_Default ? 1 : 0);
			}
			Names += FString::Printf(TEXT(" %s(v=%.2f,a=%.3f,at=%s%s)%s"), *ActiveSound->GetSound()->GetName(),
			                         ActiveSound->GetVolume(), Actual, *Location.ToCompactString(),
			                         ActiveSound->bIsOccluded ? TEXT(",occl") : TEXT(""), *Detail);
		}
		const FString AudioListener = AudioDevice->GetListeners().Num() > 0
			? AudioDevice->GetListeners()[0].Transform.GetLocation().ToCompactString() : TEXT("none");
		UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_SOUNDS frame=%d clock=%.3f captured=%.3f paused=%s dt=%.4f active=%d audible=%d focus=%s appVolume=%.2f primary=%.2f transient=%.2f muted=%s sources=%d listener=%s audioListener=%s:%s"),
		       Frame, AudioDevice->GetAudioClock(), Captured, bPaused ? TEXT("1") : TEXT("0"), GameDelta,
		       AudioDevice->GetActiveSounds().Num(), Audible, FApp::HasFocus() ? TEXT("1") : TEXT("0"),
		       FApp::GetVolumeMultiplier(), AudioDevice->GetPrimaryVolume(), AudioDevice->GetTransientPrimaryVolume(),
		       AudioDevice->IsAudioDeviceMuted() ? TEXT("1") : TEXT("0"), AudioDevice->GetNumActiveSources(),
		       *Listener, *AudioListener, *Names);
	});
}

void ARTSShortsDirector::StartSubmixCapture()
{
	FAudioDevice* AudioDevice = GetWorld()->GetAudioDeviceRaw();
	if (AudioDevice == nullptr || M_AudioCapture.IsValid())
	{
		UE_LOG(LogRTSShorts, Warning, TEXT("RTS_SHORTS_CAPTURE_SKIPPED device=%s"), AudioDevice ? TEXT("yes") : TEXT("no"));
		return;
	}
	M_AudioCapture = MakeShared<FRTSShortsAudioCapture, ESPMode::ThreadSafe>();
	AudioDevice->RegisterSubmixBufferListener(M_AudioCapture.ToSharedRef(), AudioDevice->GetMainSubmixObject());
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_CAPTURE_STARTED"));
}

void ARTSShortsDirector::StopSubmixCapture()
{
	// Whatever ends the run (finish, abort, crash-time EndPlay), the audio captured so far is kept.
	WriteSubmixCaptureWav();
	if (not M_AudioCapture.IsValid())
	{
		return;
	}
	if (FAudioDevice* AudioDevice = GetWorld() ? GetWorld()->GetAudioDeviceRaw() : nullptr)
	{
		AudioDevice->UnregisterSubmixBufferListener(M_AudioCapture.ToSharedRef(), AudioDevice->GetMainSubmixObject());
	}
	M_AudioCapture.Reset();
}

void ARTSShortsDirector::UpdateSubmixCaptureWindow(const int32 Frame)
{
	// Audio for frame F is rendered after the game tick that sees F, so at this tick the clock holds frames 0..F-1.
	// Frame 0's render is preceded by MRQ's 30 s flush, so the window is anchored on a later frame.
	if (not M_AudioCapture.IsValid() || Frame < 1)
	{
		return;
	}
	const FAudioDevice* AudioDevice = GetWorld()->GetAudioDeviceRaw();
	if (AudioDevice == nullptr)
	{
		return;
	}
	const double Clock = AudioDevice->GetAudioClock();
	const double FrameSeconds = 1.0 / FMath::Max(1, M_Job.Fps);
	if (not bM_CaptureStartClockSet)
	{
		M_CaptureStartClock = Clock - Frame * FrameSeconds;
		bM_CaptureStartClockSet = true;
		UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_CAPTURE_ANCHOR frame=%d clock=%.4f start=%.4f firstBuffer=%.4f"),
		       Frame, Clock, M_CaptureStartClock, M_AudioCapture->GetFirstBufferClock());
	}
	M_CaptureEndClock = Clock + (M_Manifest.TotalFrames - Frame) * FrameSeconds;
}

void ARTSShortsDirector::WriteSubmixCaptureWav()
{
	if (bM_CaptureWritten || not M_AudioCapture.IsValid() || not bM_CaptureStartClockSet)
	{
		return;
	}
	bM_CaptureWritten = true;
	const FString Path = M_Job.OutputDir / (M_Job.JobName + TEXT("_capture.wav"));
	FString Error;
	const double Written = M_AudioCapture->WriteWav(Path, M_CaptureStartClock, M_CaptureEndClock, Error);
	if (Written < 0.0)
	{
		UE_LOG(LogRTSShorts, Error, TEXT("RTS_SHORTS_CAPTURE_FAILED %s"), *Error);
		return;
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_CAPTURE_WRITTEN file=%s seconds=%.3f expected=%.3f captured=%.3f window=%.3f..%.3f"),
	       *Path, Written, static_cast<double>(M_Manifest.TotalFrames) / M_Job.Fps, M_AudioCapture->GetCapturedSeconds(),
	       M_CaptureStartClock, M_CaptureEndClock);
}

void ARTSShortsDirector::BeginRealtimeAudioRecording()
{
	// The audio pass runs on world time; frame 0 of the manifest is this call, so the WAV lines up by frame/fps.
	float Seconds = M_Job.WarmupSeconds;
	for (const FRTSShortsSegment& Segment : M_Job.Segments)
	{
		Seconds += Segment.SlotSeconds;
	}
	UAudioMixerBlueprintLibrary::StartRecordingOutput(this, Seconds + 2.f);
	M_AudioRecordStartWorldSeconds = GetWorld()->GetTimeSeconds();
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_AUDIO_RECORD_START seconds=%.1f"), Seconds);
}

void ARTSShortsDirector::EndRealtimeAudioRecordingAndExit()
{
	const FString Name = M_Job.JobName + TEXT("_realtime");
	UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, Name, M_Job.OutputDir);
	const double Recorded = GetWorld()->GetTimeSeconds() - M_AudioRecordStartWorldSeconds;
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_AUDIO_RECORD_DONE file=%s/%s.wav worldSeconds=%.2f"),
	       *M_Job.OutputDir, *Name, Recorded);
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_JOB_DONE job=%s success=true"), *M_Job.JobName);
	GLog->Flush();
	FPlatformMisc::RequestExitWithStatus(false, 0);
}

void ARTSShortsDirector::KeepAudioUnmuted() const
{
	// Windows sets the app volume to the unfocused multiplier on every deactivation, and a zero primary volume
	// flags every bMuteWhenBackgrounded submix as background-muted, which zeroes the buffer the recorder sees.
	FApp::SetVolumeMultiplier(1.f);
	FAudioDevice* AudioDevice = GetWorld()->GetAudioDeviceRaw();
	Audio::FMixerDevice* Mixer = AudioDevice != nullptr ? static_cast<Audio::FMixerDevice*>(AudioDevice) : nullptr;
	if (Mixer == nullptr)
	{
		return;
	}
	if (Audio::FMixerSubmixPtr Master = Mixer->GetMasterSubmix().Pin())
	{
		Master->SetBackgroundMuted(false);
	}
	for (TObjectIterator<USoundSubmix> It; It; ++It)
	{
		It->bMuteWhenBackgrounded = false;
		if (Audio::FMixerSubmixPtr Instance = Mixer->GetSubmixInstance(*It).Pin())
		{
			Instance->SetBackgroundMuted(false);
		}
	}
}

void ARTSShortsDirector::WriteManifest()
{
	if (M_Job.OutputDir.IsEmpty())
	{
		return;
	}
	FString Json;
	if (not FJsonObjectConverter::UStructToJsonObjectString(M_Manifest, Json))
	{
		UE_LOG(LogRTSShorts, Error, TEXT("RTS_SHORTS_MANIFEST_SERIALIZE_FAILED"));
		return;
	}
	const FString Path = FPaths::Combine(M_Job.OutputDir, TEXT("Manifest.json"));
	FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool ARTSShortsDirector::GetIsValidPlayerController() const
{
	if (IsValid(M_PlayerController))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_PlayerController", "GetIsValidPlayerController", this);
	return false;
}

bool ARTSShortsDirector::GetIsValidStage() const
{
	if (IsValid(M_Stage))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_Stage", "GetIsValidStage", this);
	return false;
}

bool ARTSShortsDirector::GetIsValidCamera() const
{
	if (IsValid(M_Camera))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_Camera", "GetIsValidCamera", this);
	return false;
}


void ARTSShortsDirector::BeginBattle(const int32 SegmentIndex, AActor* MainUnit)
{
	const FRTSShortsSegment& Segment = M_Job.Segments[SegmentIndex];
	M_Battle = FRTSShortsBattleState();
	M_Battle.bIsActive = true;
	M_Battle.PlayerUnits.Add(MainUnit);
	const FTransform StageTransform = M_Stage->GetActorTransform();
	const FVector Forward = StageTransform.GetRotation().GetForwardVector();
	const FVector Right = StageTransform.GetRotation().GetRightVector();
	const float Yaw = StageTransform.Rotator().Yaw + Segment.UnitYawOffset;
	// Line abreast: 0, +1, -1, +2, -2 spacings off the stage axis.
	auto LateralSlot = [](const int32 Index)
	{
		return Index == 0 ? 0.f : (Index % 2 == 1 ? 1.f : -1.f) * static_cast<float>((Index + 1) / 2);
	};
#if RTS_WITH_MAP_TESTS
	for (int32 Index = 0; Index < Segment.BattleAllyClassPaths.Num(); ++Index)
	{
		const FVector Location = GetBattleGroundPoint(StageTransform.GetLocation() + Right * LateralSlot(Index + 1) * Segment.BattleSpacing);
		AActor* Ally = FGrenadeTestHelpers::SpawnUnitForPlayer(*GetWorld(), *Segment.BattleAllyClassPaths[Index],
		                                                       FTransform(FRotator(0.f, Yaw, 0.f), Location),
		                                                       static_cast<uint8>(M_Job.UnitOwningPlayer));
		if (IsValid(Ally))
		{
			M_Battle.PlayerUnits.Add(Ally);
		}
	}
	for (int32 Index = 0; Index < Segment.BattleEnemyClassPaths.Num(); ++Index)
	{
		const FVector Location = GetBattleGroundPoint(StageTransform.GetLocation() + Forward * Segment.TargetOffset.X
		                                              + Right * LateralSlot(Index) * Segment.BattleSpacing);
		AActor* Enemy = FGrenadeTestHelpers::SpawnUnitForPlayer(*GetWorld(), *Segment.BattleEnemyClassPaths[Index],
		                                                        FTransform(FRotator(0.f, Yaw + 180.f, 0.f), Location),
		                                                        static_cast<uint8>(Segment.TargetOwningPlayer));
		if (IsValid(Enemy))
		{
			M_Battle.EnemyUnits.Add(Enemy);
		}
	}
#endif
	// The fog of war would hide the enemy line from the player and keep the player line out of the AI's vision
	// (RTSIsVisibleTarget), so neither side would fire: battle units leave the fog system and are kept visible.
	const TArray<TWeakObjectPtr<AActor>>* Sides[] = {&M_Battle.PlayerUnits, &M_Battle.EnemyUnits};
	for (const TArray<TWeakObjectPtr<AActor>>* Side : Sides)
	{
		for (const TWeakObjectPtr<AActor>& BattleUnit : *Side)
		{
			UFowComp* Fow = BattleUnit.IsValid() ? BattleUnit->FindComponentByClass<UFowComp>() : nullptr;
			if (Fow != nullptr)
			{
				Fow->StopFowParticipation();
			}
		}
	}
	KeepBattleUnitsVisible();
	M_Battle.ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &ARTSShortsDirector::OnBattleActorSpawned));
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_BATTLE_BEGIN segment=%d players=%d enemies=%d"), SegmentIndex,
	       M_Battle.PlayerUnits.Num(), M_Battle.EnemyUnits.Num());
}

void ARTSShortsDirector::OnBattleActorSpawned(AActor* SpawnedActor)
{
	if (IsValid(SpawnedActor) && SpawnedActor->GetClass()->GetName().StartsWith(TEXT("Env_Dest")))
	{
		M_Battle.SpawnedWrecks.Add(SpawnedActor);
	}
}

FVector ARTSShortsDirector::GetBattleGroundPoint(const FVector& Location) const
{
	constexpr float TraceHalfHeightCm = 2000.f;
	constexpr float SpawnLiftCm = 80.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShortsBattleGround), false);
	for (const TObjectPtr<AActor>& Unit : M_Units)
	{
		if (IsValid(Unit))
		{
			Params.AddIgnoredActor(Unit.Get());
		}
	}
	FHitResult Hit;
	const FVector Start = Location + FVector(0.f, 0.f, TraceHalfHeightCm);
	const FVector End = Location - FVector(0.f, 0.f, TraceHalfHeightCm);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		return Hit.ImpactPoint + FVector(0.f, 0.f, SpawnLiftCm);
	}
	return Location + FVector(0.f, 0.f, SpawnLiftCm);
}

namespace RTSShortsBattlePrivate
{
	bool GetIsAlive(const TWeakObjectPtr<AActor>& Unit)
	{
		return Unit.IsValid() && Unit->IsUnitAlive();
	}

	int32 CountAlive(const TArray<TWeakObjectPtr<AActor>>& Units)
	{
		int32 Alive = 0;
		for (const TWeakObjectPtr<AActor>& Unit : Units)
		{
			Alive += GetIsAlive(Unit) ? 1 : 0;
		}
		return Alive;
	}

	AActor* FindNearestAlive(const AActor* From, const TArray<TWeakObjectPtr<AActor>>& Candidates)
	{
		AActor* Nearest = nullptr;
		float NearestDistance = TNumericLimits<float>::Max();
		for (const TWeakObjectPtr<AActor>& Candidate : Candidates)
		{
			if (not GetIsAlive(Candidate))
			{
				continue;
			}
			const float Distance = FVector::DistSquared(From->GetActorLocation(), Candidate->GetActorLocation());
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				Nearest = Candidate.Get();
			}
		}
		return Nearest;
	}
}

void ARTSShortsDirector::KeepBattleUnitsVisible() const
{
	using DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision;
	for (const TWeakObjectPtr<AActor>& PlayerUnit : M_Battle.PlayerUnits)
	{
		if (PlayerUnit.IsValid() && not PlayerUnit->Tags.Contains(TagUnitInEnemyVision))
		{
			PlayerUnit->Tags.Add(TagUnitInEnemyVision);
		}
	}
	for (const TWeakObjectPtr<AActor>& EnemyUnit : M_Battle.EnemyUnits)
	{
		if (not EnemyUnit.IsValid())
		{
			continue;
		}
		if (EnemyUnit->IsHidden())
		{
			EnemyUnit->SetActorHiddenInGame(false);
		}
		TArray<AActor*> Attached;
		EnemyUnit->GetAttachedActors(Attached, true, true);
		for (AActor* Child : Attached)
		{
			if (IsValid(Child) && Child->IsHidden())
			{
				Child->SetActorHiddenInGame(false);
			}
		}
	}
}

void ARTSShortsDirector::IssueBattleOrders()
{
	using namespace RTSShortsBattlePrivate;
	auto OrderSide = [this](const TArray<TWeakObjectPtr<AActor>>& Attackers, const TArray<TWeakObjectPtr<AActor>>& Defenders)
	{
		for (const TWeakObjectPtr<AActor>& Attacker : Attackers)
		{
			if (not GetIsAlive(Attacker))
			{
				continue;
			}
			const TWeakObjectPtr<AActor>* Ordered = M_Battle.OrderedTargets.Find(Attacker);
			if (Ordered != nullptr && GetIsAlive(*Ordered))
			{
				continue;
			}
			AActor* Target = FindNearestAlive(Attacker.Get(), Defenders);
			ICommands* Commands = Cast<ICommands>(Attacker.Get());
			if (Target == nullptr || Commands == nullptr)
			{
				continue;
			}
			Commands->AttackActor(Target, true);
			M_Battle.OrderedTargets.Add(Attacker, Target);
		}
	};
	OrderSide(M_Battle.PlayerUnits, M_Battle.EnemyUnits);
	OrderSide(M_Battle.EnemyUnits, M_Battle.PlayerUnits);
}

void ARTSShortsDirector::TickBattle(const int32 Frame)
{
	using namespace RTSShortsBattlePrivate;
	if (not M_Battle.bIsActive || not M_Job.Segments.IsValidIndex(M_CurrentSegment))
	{
		return;
	}
	const FRTSShortsSegment& Segment = M_Job.Segments[M_CurrentSegment];
	KeepBattleUnitsVisible();
	constexpr int32 OrderEveryFrames = 20;
	if (bM_FireOrdered && Frame % OrderEveryFrames == 0)
	{
		IssueBattleOrders();
	}
	const int32 PlayersAlive = CountAlive(M_Battle.PlayerUnits);
	const int32 EnemiesAlive = CountAlive(M_Battle.EnemyUnits);
	if (M_Battle.DecidedFrame < 0 && (PlayersAlive == 0 || EnemiesAlive == 0))
	{
		M_Battle.DecidedFrame = Frame;
		M_Battle.bPlayerWon = PlayersAlive > 0;
		UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_BATTLE_DECIDED segment=%d frame=%d local=%d winner=%s survivors=%d"),
		       M_CurrentSegment, Frame, Frame - M_SegmentStartFrames[M_CurrentSegment],
		       M_Battle.bPlayerWon ? TEXT("player") : TEXT("enemy"), FMath::Max(PlayersAlive, EnemiesAlive));
	}
	const int32 HoldFrames = FMath::RoundToInt(Segment.BattleEndHoldSeconds * M_Job.Fps);
	if (M_Battle.DecidedFrame >= 0 && not M_Battle.bSegmentShortened && Frame >= M_Battle.DecidedFrame + HoldFrames)
	{
		M_Battle.bSegmentShortened = true;
		ShortenRunningSegment(Frame + 1);
	}
}

void ARTSShortsDirector::ShortenRunningSegment(const int32 Frame)
{
	if (not M_SegmentStartFrames.IsValidIndex(M_CurrentSegment + 1))
	{
		return;
	}
	const int32 Saved = M_SegmentStartFrames[M_CurrentSegment + 1] - Frame;
	if (Saved <= 0)
	{
		return;
	}
	for (int32 Index = M_CurrentSegment + 1; Index < M_SegmentStartFrames.Num(); ++Index)
	{
		M_SegmentStartFrames[Index] -= Saved;
	}
	for (int32 Index = M_CurrentSegment; Index < M_Manifest.Segments.Num(); ++Index)
	{
		M_Manifest.Segments[Index].StartFrame = M_SegmentStartFrames[Index];
		M_Manifest.Segments[Index].EndFrame = M_SegmentStartFrames[Index + 1];
	}
	M_Manifest.TotalFrames -= Saved;
	bM_TimelineShortened = true;
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_SEGMENT_SHORTENED segment=%d end=%d saved=%d"), M_CurrentSegment, Frame, Saved);
	WriteManifest();
}

void ARTSShortsDirector::EndBattle()
{
	if (not M_Battle.bIsActive)
	{
		return;
	}
	if (M_Battle.ActorSpawnedHandle.IsValid())
	{
		GetWorld()->RemoveOnActorSpawnedHandler(M_Battle.ActorSpawnedHandle);
	}
	UE_LOG(LogRTSShorts, Display, TEXT("RTS_SHORTS_BATTLE_END segment=%d decided=%d winner=%s wrecks=%d"), M_CurrentSegment,
	       M_Battle.DecidedFrame,
	       M_Battle.DecidedFrame < 0 ? TEXT("none") : (M_Battle.bPlayerWon ? TEXT("player") : TEXT("enemy")),
	       M_Battle.SpawnedWrecks.Num());
	// The segment's own unit is parked by EndSegment; everything else from this battle leaves the field.
	const AActor* MainUnit = M_Battle.PlayerUnits.Num() > 0 ? M_Battle.PlayerUnits[0].Get() : nullptr;
	const TArray<TWeakObjectPtr<AActor>>* Groups[] = {&M_Battle.PlayerUnits, &M_Battle.EnemyUnits, &M_Battle.SpawnedWrecks};
	for (const TArray<TWeakObjectPtr<AActor>>* Group : Groups)
	{
		for (const TWeakObjectPtr<AActor>& Actor : *Group)
		{
			if (Actor.IsValid() && Actor.Get() != MainUnit)
			{
				Actor->Destroy();
			}
		}
	}
	M_Battle = FRTSShortsBattleState();
}
