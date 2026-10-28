// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShortsDirector.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckTurretTossComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsCookOff, Log, All);

namespace RTSShortsCookOffPrivate
{
	bool GetIsCookOffSegment(const FRTSShortsSegment& Segment)
	{
		return Segment.CookOff.StartAtSeconds >= 0.f;
	}

	/**
	 * Only wreck parts are removed after a death: the first fire also spawns the fire subsystem's manager actor, and
	 * destroying it would leave every later cook-off without its fires.
	 */
	bool GetIsWreckActor(const AActor& Actor)
	{
		if (Actor.FindComponentByClass<UWreckTurretTossComponent>() != nullptr)
		{
			return true;
		}
		const FString ClassName = Actor.GetClass()->GetName();
		const TCHAR* const WreckWords[] = {TEXT("Wreck"), TEXT("Destroyed"), TEXT("Debris"), TEXT("Collapse")};
		for (const TCHAR* const Word : WreckWords)
		{
			if (ClassName.Contains(Word))
			{
				return true;
			}
		}
		return false;
	}

	/** The wreck's turret while the toss simulates it: the only static mesh of a tossing wreck that simulates physics. */
	UStaticMeshComponent* FindFlyingTurret(const AActor& Wreck)
	{
		if (Wreck.FindComponentByClass<UWreckTurretTossComponent>() == nullptr)
		{
			return nullptr;
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes(&Wreck);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (IsValid(Mesh) && Mesh->IsSimulatingPhysics())
			{
				return Mesh;
			}
		}
		return nullptr;
	}
}

void ARTSShortsDirector::BeginCookOff(const int32 SegmentIndex)
{
	using namespace RTSShortsCookOffPrivate;
	if (not M_Job.Segments.IsValidIndex(SegmentIndex) || not GetIsCookOffSegment(M_Job.Segments[SegmentIndex]))
	{
		return;
	}
	M_CookOff = FRTSShortsCookOffState();
	M_CookOff.bIsActive = true;
	M_CookOff.EffectIdsAtBegin = CollectActiveEffectIds();
	// FrameCamera has framed the tank: its aim point is where the camera looks at the tank's distance.
	if (GetIsValidCamera() && IsValid(M_Units[SegmentIndex].Get()))
	{
		const FVector CameraLocation = M_Camera->GetActorLocation();
		const float TankDistance = FVector::Dist(CameraLocation, M_Units[SegmentIndex]->GetActorLocation());
		M_CookOff.BaseLookAt = CameraLocation + M_Camera->GetActorForwardVector() * TankDistance;
		M_CookOff.CameraLookAt = M_CookOff.BaseLookAt;
	}
	M_CookOff.ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &ARTSShortsDirector::OnCookOffActorSpawned));
	UE_LOG(LogRTSShortsCookOff, Display, TEXT("RTS_SHORTS_COOKOFF_SEGMENT segment=%d unit=%s"), SegmentIndex,
	       *GetNameSafe(M_Units[SegmentIndex].Get()));
}

void ARTSShortsDirector::OnCookOffActorSpawned(AActor* SpawnedActor)
{
	// Only what the death leaves behind: the wreck, its debris and effects actors.
	if (M_CookOff.bIsActive && M_CookOff.DeathLocalFrame >= 0 && IsValid(SpawnedActor))
	{
		M_CookOff.SpawnedActors.Add(SpawnedActor);
	}
}

void ARTSShortsDirector::TickCookOff(const int32 LocalFrame)
{
	if (not M_CookOff.bIsActive || not M_Job.Segments.IsValidIndex(M_CurrentSegment))
	{
		return;
	}
	const FRTSShortsCookOff& CookOff = M_Job.Segments[M_CurrentSegment].CookOff;
	ATankMaster* Tank = Cast<ATankMaster>(M_Units[M_CurrentSegment].Get());
	if (not M_CookOff.bCookOffStarted && LocalFrame >= FMath::RoundToInt(CookOff.StartAtSeconds * M_Job.Fps))
	{
		M_CookOff.bCookOffStarted = true;
		if (IsValid(Tank))
		{
			Tank->Debug_StartAmmoCookOff();
		}
		UE_LOG(LogRTSShortsCookOff, Display, TEXT("RTS_SHORTS_COOKOFF_START local=%d tank=%d"), LocalFrame, IsValid(Tank) ? 1 : 0);
	}
	const int32 DeathFrame = FMath::RoundToInt((CookOff.StartAtSeconds + CookOff.DieAfterSeconds) * M_Job.Fps);
	if (M_CookOff.DeathLocalFrame < 0 && LocalFrame >= DeathFrame)
	{
		// Set before the death so the wreck it spawns is recorded.
		M_CookOff.DeathLocalFrame = LocalFrame;
		if (IsValid(Tank))
		{
			Tank->Debug_DieDuringAmmoCookOff();
		}
		// Which tanks still wait in the pen (the last one of the first job vanished without a logged death).
		FString Waiting;
		for (int32 Index = M_CurrentSegment + 1; Index < M_Units.Num(); ++Index)
		{
			Waiting += IsValid(M_Units[Index].Get()) ? TEXT("1") : TEXT("0");
		}
		UE_LOG(LogRTSShortsCookOff, Display, TEXT("RTS_SHORTS_COOKOFF_DEATH local=%d tank=%d waiting=%s"), LocalFrame,
		       IsValid(Tank) ? 1 : 0, *Waiting);
	}
	if (M_CookOff.DeathLocalFrame >= 0)
	{
		TrackTossedTurret(LocalFrame);
		AimCookOffCamera(CookOff);
	}
}

void ARTSShortsDirector::AimCookOffCamera(const FRTSShortsCookOff& CookOff)
{
	if (not GetIsValidCamera())
	{
		return;
	}
	const UStaticMeshComponent* Turret = M_CookOff.TossedTurret.Get();
	FVector AimShift = IsValid(Turret)
		? (Turret->GetComponentLocation() - M_CookOff.BaseLookAt) * CookOff.TurretAimBlend
		: FVector::ZeroVector;
	// Up is free (the turret's flight), sideways is capped: a turret thrown 25 m pulled the burning hull out of frame.
	constexpr float MaxSidewaysAimShiftCm = 500.f;
	const FVector SidewaysShift = FVector(AimShift.X, AimShift.Y, 0.f).GetClampedToMaxSize(MaxSidewaysAimShiftCm);
	AimShift = FVector(SidewaysShift.X, SidewaysShift.Y, AimShift.Z);
	const FVector Target = M_CookOff.BaseLookAt + AimShift;
	const float FrameSeconds = 1.f / FMath::Max(1, M_Job.Fps);
	M_CookOff.CameraLookAt = FMath::VInterpTo(M_CookOff.CameraLookAt, Target, FrameSeconds, CookOff.AimTrackSpeed);
	const FVector CameraLocation = M_Camera->GetActorLocation();
	M_Camera->SetActorRotation((M_CookOff.CameraLookAt - CameraLocation).Rotation());
}

void ARTSShortsDirector::TrackTossedTurret(const int32 LocalFrame)
{
	using namespace RTSShortsCookOffPrivate;
	const FRTSShortsCookOff& CookOff = M_Job.Segments[M_CurrentSegment].CookOff;
	if (not M_CookOff.bTurretFlying && M_CookOff.LandedLocalFrame < 0)
	{
		for (const TWeakObjectPtr<AActor>& Spawned : M_CookOff.SpawnedActors)
		{
			UStaticMeshComponent* Turret = Spawned.IsValid() ? FindFlyingTurret(*Spawned.Get()) : nullptr;
			if (IsValid(Turret))
			{
				M_CookOff.TossedTurret = Turret;
				M_CookOff.bTurretFlying = true;
				M_CookOff.MaxTurretHeightCm = Turret->GetComponentLocation().Z;
				UE_LOG(LogRTSShortsCookOff, Display, TEXT("RTS_SHORTS_COOKOFF_TURRET_LAUNCH local=%d wreck=%s"), LocalFrame,
				       *GetNameSafe(Spawned.Get()));
				break;
			}
		}
	}
	const UStaticMeshComponent* Turret = M_CookOff.TossedTurret.Get();
	if (M_CookOff.bTurretFlying && IsValid(Turret))
	{
		M_CookOff.MaxTurretHeightCm = FMath::Max(M_CookOff.MaxTurretHeightCm, Turret->GetComponentLocation().Z);
	}
	// Landed: back down from its peak and nearly at rest. The toss component only switches physics off seconds later,
	// after the turret has lain still for a while; that (or a lost turret) also counts.
	constexpr float DescendedBelowPeakCm = 250.f;
	constexpr float RestingSpeedCmPerSecond = 250.f;
	const bool bCameDown = IsValid(Turret) && Turret->IsSimulatingPhysics()
		&& Turret->GetComponentLocation().Z < M_CookOff.MaxTurretHeightCm - DescendedBelowPeakCm
		&& Turret->GetComponentVelocity().Size() < RestingSpeedCmPerSecond;
	if (M_CookOff.bTurretFlying && (bCameDown || not IsValid(Turret) || not Turret->IsSimulatingPhysics()))
	{
		M_CookOff.bTurretFlying = false;
		M_CookOff.LandedLocalFrame = LocalFrame;
		UE_LOG(LogRTSShortsCookOff, Display, TEXT("RTS_SHORTS_COOKOFF_TURRET_LANDED local=%d afterDeath=%.2f peakZ=%.0f"),
		       LocalFrame, static_cast<float>(LocalFrame - M_CookOff.DeathLocalFrame) / M_Job.Fps, M_CookOff.MaxTurretHeightCm);
	}
	const int32 SinceDeath = LocalFrame - M_CookOff.DeathLocalFrame;
	const bool bHullOnly = not M_CookOff.bTurretFlying && M_CookOff.LandedLocalFrame < 0
		&& SinceDeath >= FMath::RoundToInt(CookOff.HullOnlyHoldSeconds * M_Job.Fps);
	const bool bHeldAfterLanding = M_CookOff.LandedLocalFrame >= 0
		&& LocalFrame >= M_CookOff.LandedLocalFrame + FMath::RoundToInt(CookOff.HoldAfterLandSeconds * M_Job.Fps);
	const bool bTimedOut = SinceDeath >= FMath::RoundToInt(CookOff.MaxAfterDeathSeconds * M_Job.Fps);
	if (not M_CookOff.bSegmentShortened && (bHullOnly || bHeldAfterLanding || bTimedOut))
	{
		M_CookOff.bSegmentShortened = true;
		UE_LOG(LogRTSShortsCookOff, Display, TEXT("RTS_SHORTS_COOKOFF_END local=%d reason=%s"), LocalFrame,
		       bHeldAfterLanding ? TEXT("landed") : bHullOnly ? TEXT("hull_only") : TEXT("timeout"));
		ShortenRunningSegment(GetTimelineFrame());
	}
}

void ARTSShortsDirector::EndCookOff()
{
	if (not M_CookOff.bIsActive)
	{
		return;
	}
	if (M_CookOff.ActorSpawnedHandle.IsValid())
	{
		GetWorld()->RemoveOnActorSpawnedHandler(M_CookOff.ActorSpawnedHandle);
	}
	// The wreck and its burning turret would otherwise lie beside the next tank.
	int32 Removed = 0;
	for (const TWeakObjectPtr<AActor>& Spawned : M_CookOff.SpawnedActors)
	{
		AActor* Actor = Spawned.Get();
		if (IsValid(Actor) && RTSShortsCookOffPrivate::GetIsWreckActor(*Actor))
		{
			Actor->Destroy();
			++Removed;
		}
	}
	const int32 Stopped = StopEffectsStartedSince(M_CookOff.EffectIdsAtBegin);
	UE_LOG(LogRTSShortsCookOff, Display, TEXT("RTS_SHORTS_COOKOFF_CLEARED segment=%d actors=%d effects=%d"), M_CurrentSegment,
	       Removed, Stopped);
	M_CookOff = FRTSShortsCookOffState();
}
