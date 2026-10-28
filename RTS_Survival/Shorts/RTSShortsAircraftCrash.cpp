// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShortsDirector.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "UObject/UObjectIterator.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftDeath/AircraftDeathComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftDeath/AircraftWreckActor.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsAircraftCrash, Log, All);

namespace RTSShortsAircraftCrashPrivate
{
	const TCHAR* const KindName = TEXT("AircraftCrash");
	// The run never ends on its own: the aircraft is shot down long before it covers this distance.
	constexpr float RunLengthCm = 200000.f;
	// The crash camera aims a little above the impact point, at the fireball rather than the ground.
	constexpr float ImpactAimHeightCm = 250.f;

	/** Ground below Location (WorldStatic, like the aircraft's own impact trace); Location itself if none. */
	FVector FindGroundBelow(const UWorld& World, const FVector& Location, const AActor* IgnoredActor)
	{
		constexpr float TraceDepthCm = 200000.f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ShortsAircraftCrashGround), false, IgnoredActor);
		FHitResult Hit;
		if (World.LineTraceSingleByChannel(Hit, Location, Location - FVector(0.f, 0.f, TraceDepthCm), ECC_WorldStatic, Params))
		{
			return Hit.ImpactPoint;
		}
		return Location;
	}
}

bool ARTSShortsDirector::GetIsAircraftCrashSegment(const int32 SegmentIndex) const
{
	return M_Job.Segments.IsValidIndex(SegmentIndex)
		&& M_Job.Segments[SegmentIndex].Kind.Equals(RTSShortsAircraftCrashPrivate::KindName, ESearchCase::IgnoreCase);
}

void ARTSShortsDirector::BeginAircraftCrash(const int32 SegmentIndex)
{
	using namespace RTSShortsAircraftCrashPrivate;
	if (not GetIsAircraftCrashSegment(SegmentIndex) || not GetIsValidStage() || not GetIsValidCamera())
	{
		return;
	}
	const FRTSShortsSegment& Segment = M_Job.Segments[SegmentIndex];
	const FRTSShortsAircraftCrash& Crash = Segment.AircraftCrash;
	const FTransform StageTransform = M_Stage->GetActorTransform();
	const FVector StartLocation = StageTransform.TransformPosition(Crash.StartOffset);
	const FRotator RunRotation(0.f, M_Stage->GetActorRotation().Yaw + Crash.RunYaw, 0.f);
	AAircraftMaster* Aircraft = SpawnCrashAircraft(Segment, FTransform(RunRotation, StartLocation));
	if (not IsValid(Aircraft))
	{
		MarkSegmentSkipped(SegmentIndex, TEXT("crash aircraft spawn failed: ") + Segment.ClassPath);
		return;
	}
	const FVector RunDirection = RunRotation.Vector();
	const FVector RunEnd = StartLocation + RunDirection * RunLengthCm;
	Aircraft->OrderStraightFlightRun(RunEnd, RunEnd);
	Aircraft->SetStraightRunHoldAtEnd(true);
	// Spawned after the job's units: switch the distance optimisation off again so it covers this aircraft too.
	DisableUnitOptimisation();
	PrepareParatrooperAircraftForCapture(Aircraft);

	M_AircraftCrash = FRTSShortsAircraftCrashState();
	M_AircraftCrash.EffectIdsAtBegin = CollectActiveEffectIds();
	M_AircraftCrash.Aircraft = Aircraft;
	M_AircraftCrash.RunRotation = RunRotation;
	M_AircraftCrash.bIsActive = true;
	M_Orbit.bIsActive = false;
	ApplyCrashCameraGrade(Segment.Camera);
	UpdateCrashFollowCamera(*Aircraft, Crash);
	ApplyCameraCut(M_Camera->GetActorLocation());
	UE_LOG(LogRTSShortsAircraftCrash, Display, TEXT("RTS_SHORTS_AIRCRAFT_CRASH_BEGIN segment=%d aircraft=%s start=%s yaw=%.0f"),
	       SegmentIndex, *Aircraft->GetName(), *StartLocation.ToCompactString(), RunRotation.Yaw);
}

AAircraftMaster* ARTSShortsDirector::SpawnCrashAircraft(const FRTSShortsSegment& Segment, const FTransform& SpawnTransform) const
{
	// Not FGrenadeTestHelpers::SpawnUnitForPlayer: it drops the unit to the ground, and the run would be flown there.
	UClass* AircraftClass = LoadClass<AActor>(nullptr, *Segment.ClassPath);
	if (not IsValid(AircraftClass))
	{
		return nullptr;
	}
	AActor* Spawned = GetWorld()->SpawnActorDeferred<AActor>(AircraftClass, SpawnTransform, nullptr, nullptr,
	                                                         ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(Spawned))
	{
		return nullptr;
	}
	// The owner must be set before BeginPlay, so the aircraft registers as that player's unit.
	if (URTSComponent* RTSComponent = Spawned->FindComponentByClass<URTSComponent>())
	{
		RTSComponent->SetOwningPlayerRuntime(static_cast<uint8>(M_Job.UnitOwningPlayer));
	}
	Spawned->FinishSpawning(SpawnTransform);
	AAircraftMaster* Aircraft = Cast<AAircraftMaster>(Spawned);
	if (IsValid(Spawned) && not IsValid(Aircraft))
	{
		UE_LOG(LogRTSShortsAircraftCrash, Error, TEXT("RTS_SHORTS_AIRCRAFT_CRASH_NOT_AIRCRAFT class=%s"), *Segment.ClassPath);
		Spawned->Destroy();
		return nullptr;
	}
	if (IsValid(Aircraft) && Aircraft->GetController() == nullptr)
	{
		Aircraft->SpawnDefaultController();
	}
	return Aircraft;
}

void ARTSShortsDirector::ApplyCrashCameraGrade(const FRTSShortsCameraParams& Params) const
{
	// FrameCamera sets these for unit segments; a crash segment never frames a unit, so it applies the grade itself.
	UCineCameraComponent* CineComponent = GetIsValidCamera() ? M_Camera->GetCineCameraComponent() : nullptr;
	if (not IsValid(CineComponent))
	{
		return;
	}
	CineComponent->FocusSettings.FocusMethod = ECameraFocusMethod::Disable;
	CineComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	CineComponent->PostProcessSettings.AutoExposureBias = Params.ExposureBias;
	CineComponent->PostProcessSettings.bOverride_MotionBlurAmount = true;
	CineComponent->PostProcessSettings.MotionBlurAmount = 0.f;
	CineComponent->PostProcessSettings.bOverride_ColorSaturation = true;
	CineComponent->PostProcessSettings.ColorSaturation = FVector4(Params.ColorSaturation, Params.ColorSaturation, Params.ColorSaturation, 1.f);
	CineComponent->PostProcessSettings.bOverride_WhiteTemp = not FMath::IsNearlyEqual(Params.WhiteTemp, 6500.f);
	CineComponent->PostProcessSettings.WhiteTemp = Params.WhiteTemp;
	CineComponent->PostProcessBlendWeight = 1.f;
}

void ARTSShortsDirector::TickAircraftCrash(const int32 LocalFrame)
{
	if (not M_AircraftCrash.bIsActive || not M_Job.Segments.IsValidIndex(M_CurrentSegment) || not GetIsValidCamera())
	{
		return;
	}
	const FRTSShortsAircraftCrash& Crash = M_Job.Segments[M_CurrentSegment].AircraftCrash;
	AAircraftMaster* Aircraft = M_AircraftCrash.Aircraft.Get();
	if (IsValid(Aircraft))
	{
		PrepareParatrooperAircraftForCapture(Aircraft);
		if (not M_AircraftCrash.bKilled && LocalFrame >= FMath::RoundToInt(Crash.KillAtSeconds * M_Job.Fps))
		{
			KillCrashAircraft(*Aircraft, LocalFrame);
		}
		const int32 CutFrame = FMath::RoundToInt((Crash.KillAtSeconds + Crash.CrashViewAfterKillSeconds) * M_Job.Fps);
		if (not M_AircraftCrash.bCrashViewCut && LocalFrame >= CutFrame)
		{
			CutToCrashView(*Aircraft, Crash);
		}
	}
	TrackCrashImpact(LocalFrame, Crash);
	if (M_AircraftCrash.bCrashViewCut)
	{
		UpdateCrashViewCamera(Crash);
	}
	else if (IsValid(Aircraft))
	{
		UpdateCrashFollowCamera(*Aircraft, Crash);
	}
	// Distance based optimisation and sound measure from the player's pawn: it rides along with the chase camera.
	if (APawn* Pawn = GetIsValidPlayerController() ? M_PlayerController->GetPawn() : nullptr)
	{
		Pawn->SetActorLocation(M_Camera->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	const int32 LogIntervalFrames = FMath::Max(1, M_Job.Fps / 2);
	if (LocalFrame % LogIntervalFrames == 0 && IsValid(Aircraft))
	{
		UE_LOG(LogRTSShortsAircraftCrash, Display, TEXT("RTS_SHORTS_AIRCRAFT_CRASH_FLIGHT local=%d at=%s rotation=%s camera=%s"),
		       LocalFrame, *Aircraft->GetActorLocation().ToCompactString(), *Aircraft->GetActorRotation().ToCompactString(),
		       *M_Camera->GetActorLocation().ToCompactString());
	}
}

void ARTSShortsDirector::KillCrashAircraft(AAircraftMaster& Aircraft, const int32 LocalFrame)
{
	M_AircraftCrash.bKilled = true;
	Aircraft.TakeFatalDamage();
	UE_LOG(LogRTSShortsAircraftCrash, Display, TEXT("RTS_SHORTS_AIRCRAFT_CRASH_KILLED local=%d at=%s velocity=%s"),
	       LocalFrame, *Aircraft.GetActorLocation().ToCompactString(), *Aircraft.GetVelocity().ToCompactString());
}

void ARTSShortsDirector::UpdateCrashFollowCamera(const AAircraftMaster& Aircraft, const FRTSShortsAircraftCrash& Crash) const
{
	if (not GetIsValidCamera())
	{
		return;
	}
	const FVector AircraftLocation = Aircraft.GetActorLocation();
	const FVector CameraLocation = AircraftLocation + M_AircraftCrash.RunRotation.RotateVector(Crash.FollowOffset);
	const FVector LookAt = AircraftLocation + M_AircraftCrash.RunRotation.Vector() * Crash.FollowLookAhead;
	M_Camera->SetActorLocationAndRotation(CameraLocation, (LookAt - CameraLocation).Rotation());
	M_Camera->GetCineCameraComponent()->SetCurrentFocalLength(Crash.FollowFocalLength);
}

void ARTSShortsDirector::CutToCrashView(const AAircraftMaster& Aircraft, const FRTSShortsAircraftCrash& Crash)
{
	using namespace RTSShortsAircraftCrashPrivate;
	if (not GetIsValidCamera())
	{
		return;
	}
	M_AircraftCrash.bCrashViewCut = true;
	const FVector Ground = FindGroundBelow(*GetWorld(), Aircraft.GetActorLocation(), &Aircraft);
	const FVector PlanarOffset = M_AircraftCrash.RunRotation.RotateVector(FVector(Crash.CrashViewOffset.X, Crash.CrashViewOffset.Y, 0.f));
	M_AircraftCrash.CrashCameraLocation = Ground + PlanarOffset + FVector(0.f, 0.f, Crash.CrashViewOffset.Z);
	M_AircraftCrash.CrashCameraLookAt = Aircraft.GetActorLocation();
	M_Camera->SetActorLocationAndRotation(M_AircraftCrash.CrashCameraLocation,
	                                      (M_AircraftCrash.CrashCameraLookAt - M_AircraftCrash.CrashCameraLocation).Rotation());
	M_Camera->GetCineCameraComponent()->SetCurrentFocalLength(Crash.CrashViewFocalLength);
	ApplyCameraCut(M_AircraftCrash.CrashCameraLocation);
	UE_LOG(LogRTSShortsAircraftCrash, Display, TEXT("RTS_SHORTS_AIRCRAFT_CRASH_CUT camera=%s ground=%s aircraft=%s"),
	       *M_AircraftCrash.CrashCameraLocation.ToCompactString(), *Ground.ToCompactString(),
	       *Aircraft.GetActorLocation().ToCompactString());
}

void ARTSShortsDirector::UpdateCrashViewCamera(const FRTSShortsAircraftCrash& Crash)
{
	using namespace RTSShortsAircraftCrashPrivate;
	if (not GetIsValidCamera())
	{
		return;
	}
	const AAircraftMaster* Aircraft = M_AircraftCrash.Aircraft.Get();
	const bool bImpacted = M_AircraftCrash.ImpactLocalFrame >= 0;
	if (not bImpacted && not IsValid(Aircraft))
	{
		return;
	}
	const FVector Target = bImpacted
		? M_AircraftCrash.ImpactLocation + FVector(0.f, 0.f, ImpactAimHeightCm)
		: Aircraft->GetActorLocation();
	const float FrameSeconds = 1.f / FMath::Max(1, M_Job.Fps);
	M_AircraftCrash.CrashCameraLookAt = FMath::VInterpTo(M_AircraftCrash.CrashCameraLookAt, Target, FrameSeconds, Crash.CrashViewTrackSpeed);
	M_Camera->SetActorLocationAndRotation(M_AircraftCrash.CrashCameraLocation,
	                                      (M_AircraftCrash.CrashCameraLookAt - M_AircraftCrash.CrashCameraLocation).Rotation());
}

void ARTSShortsDirector::TrackCrashImpact(const int32 LocalFrame, const FRTSShortsAircraftCrash& Crash)
{
	if (M_AircraftCrash.ImpactLocalFrame < 0)
	{
		const AAircraftMaster* Aircraft = M_AircraftCrash.Aircraft.Get();
		const UAircraftDeathComponent* Death = IsValid(Aircraft) ? Aircraft->GetDeathComponent() : nullptr;
		if (not IsValid(Death) || Death->GetDeathPhase() != EAircraftDeathPhase::Impacted)
		{
			return;
		}
		M_AircraftCrash.ImpactLocalFrame = LocalFrame;
		M_AircraftCrash.ImpactLocation = Death->GetImpactLocation();
		UE_LOG(LogRTSShortsAircraftCrash, Display, TEXT("RTS_SHORTS_AIRCRAFT_CRASH_IMPACT local=%d seconds=%.2f at=%s"),
		       LocalFrame, static_cast<float>(LocalFrame) / M_Job.Fps, *M_AircraftCrash.ImpactLocation.ToCompactString());
	}
	const int32 EndFrame = M_AircraftCrash.ImpactLocalFrame + FMath::RoundToInt(Crash.HoldAfterImpactSeconds * M_Job.Fps);
	if (not M_AircraftCrash.bSegmentShortened && LocalFrame >= EndFrame)
	{
		M_AircraftCrash.bSegmentShortened = true;
		ShortenRunningSegment(GetTimelineFrame());
	}
}

void ARTSShortsDirector::EndAircraftCrash()
{
	if (not M_AircraftCrash.bIsActive)
	{
		return;
	}
	UE_LOG(LogRTSShortsAircraftCrash, Display, TEXT("RTS_SHORTS_AIRCRAFT_CRASH_END segment=%d killed=%d cut=%d impactLocal=%d"),
	       M_CurrentSegment, M_AircraftCrash.bKilled ? 1 : 0, M_AircraftCrash.bCrashViewCut ? 1 : 0, M_AircraftCrash.ImpactLocalFrame);
	if (AAircraftMaster* Aircraft = M_AircraftCrash.Aircraft.Get())
	{
		Aircraft->Destroy();
	}
	// The burning wreck would otherwise still lie in the next aircraft's shot.
	for (TActorIterator<AAircraftWreckActor> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	// The smoke trail puffs and the explosion would otherwise hang in the next aircraft's shot.
	const int32 Stopped = StopEffectsStartedSince(M_AircraftCrash.EffectIdsAtBegin);
	UE_LOG(LogRTSShortsAircraftCrash, Display, TEXT("RTS_SHORTS_AIRCRAFT_CRASH_EFFECTS_STOPPED count=%d"), Stopped);
	M_AircraftCrash = FRTSShortsAircraftCrashState();
}

TSet<uint32> ARTSShortsDirector::CollectActiveEffectIds() const
{
	TSet<uint32> ActiveIds;
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		// Only active ones: crash smoke and explosions come from a component pool, and a pooled component released by
		// an earlier segment already exists, inactive, when the next one begins.
		if (It->GetWorld() == GetWorld() && It->IsActive())
		{
			ActiveIds.Add(It->GetUniqueID());
		}
	}
	return ActiveIds;
}

int32 ARTSShortsDirector::StopEffectsStartedSince(const TSet<uint32>& EffectIdsAtBegin) const
{
	int32 Stopped = 0;
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* Component = *It;
		if (Component->GetWorld() != GetWorld() || not Component->IsActive()
			|| EffectIdsAtBegin.Contains(Component->GetUniqueID())
			|| GetIsPersistentEffectOwner(Component->GetOwner()))
		{
			continue;
		}
		Component->DeactivateImmediate();
		++Stopped;
	}
	return Stopped;
}
