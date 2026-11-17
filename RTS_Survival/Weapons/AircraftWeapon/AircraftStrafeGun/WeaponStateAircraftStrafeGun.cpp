// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WeaponStateAircraftStrafeGun.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Weapons/AircraftWeapon/AircraftStrafeGun/AircraftStrafeShotRecorder.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h"

namespace AircraftStrafeGunPrivate
{
	// Normal distribution sample clamped to +-MaxSigma (Box-Muller).
	float GetClampedGaussian(const float MaxSigma)
	{
		const float UniformA = FMath::Max(FMath::FRand(), UE_SMALL_NUMBER);
		const float UniformB = FMath::FRand();
		const float Gaussian = FMath::Sqrt(-2.f * FMath::Loge(UniformA)) * FMath::Cos(2.f * PI * UniformB);
		return FMath::Clamp(Gaussian, -MaxSigma, MaxSigma);
	}

	FVector GetFlatDirection(const FVector& Direction, const FVector& Fallback)
	{
		const FVector Flat = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal();
		return Flat.IsNearlyZero() ? Fallback : Flat;
	}
}

bool UWeaponStateAircraftStrafeGun::GetIsLegacyAircraftGunsRequested()
{
	static const bool bIsLegacyRequested = FParse::Param(FCommandLine::Get(), TEXT("AircraftStrafeLegacyGuns"));
	return bIsLegacyRequested;
}

void UWeaponStateAircraftStrafeGun::FireWeaponSystem()
{
	if (GetShouldUseRegularMultiTrace())
	{
		Super::FireWeaponSystem();
		return;
	}
	FStrafeGunAim Aim;
	if (not ComputeStrafeGunAim(Aim) || not ValidateAndCacheSockets())
	{
		Super::FireWeaponSystem();
		return;
	}
	for (const FName& SocketName : M_FireSocketNames)
	{
		FireRoundFromSocket(SocketName, Aim);
	}
}

bool UWeaponStateAircraftStrafeGun::GetShouldUseRegularMultiTrace() const
{
	if (GetIsLegacyAircraftGunsRequested() || not WeaponOwner.GetObject())
	{
		return true;
	}
	// Air-to-air: the guns keep converging on the other aircraft.
	return IsValid(Cast<AAircraftMaster>(WeaponOwner->GetTargetActor(WeaponIndex)));
}

bool UWeaponStateAircraftStrafeGun::ComputeStrafeGunAim(FStrafeGunAim& OutAim) const
{
	using namespace AircraftStrafeGunSettings;
	if (not IsValid(World) || not IsValid(MeshComponent) || not IsValid(MeshComponent->GetOwner()))
	{
		return false;
	}
	const FRotator NoseRotation = MeshComponent->GetOwner()->GetActorRotation();
	// Reading the fire direction advances the owner's aim point cycling over an actor target; the direction itself
	// is replaced by the gun line below.
	(void)WeaponOwner->GetFireDirection(WeaponIndex);
	const FVector TargetLocation = WeaponOwner->GetTargetLocation(WeaponIndex);
	OutAim.GunsCentre = GetGunsCentre();
	const FVector DesiredDirection = (TargetLocation - OutAim.GunsCentre).GetSafeNormal();
	OutAim.GunLineDirection = DesiredDirection.IsNearlyZero()
		                          ? NoseRotation.Vector()
		                          : ClampDirectionToNoseCone(DesiredDirection, NoseRotation);
	OutAim.FlightForward2D = AircraftStrafeGunPrivate::GetFlatDirection(
		NoseRotation.Vector(), AircraftStrafeGunPrivate::GetFlatDirection(OutAim.GunLineDirection, FVector::ForwardVector));
	OutAim.FlightRight2D = FVector::CrossProduct(FVector::UpVector, OutAim.FlightForward2D);

	const float Reach = WeaponData.Range * DeveloperSettings::GameBalance::Ranges::TraceSimulationRangeMlt
		* GetFlightRangeMltForCurrentTarget();
	const float HeightAboveAimPlane = OutAim.GunsCentre.Z - TargetLocation.Z;
	const float DescentPerCm = -OutAim.GunLineDirection.Z;
	const float DistanceToAimPlane = DescentPerCm > UE_KINDA_SMALL_NUMBER && HeightAboveAimPlane > 0.f
		                                 ? HeightAboveAimPlane / DescentPerCm
		                                 : TNumericLimits<float>::Max();
	OutAim.bReachesGround = DistanceToAimPlane <= Reach;
	OutAim.SlantDistance = FMath::Min(DistanceToAimPlane, Reach);
	OutAim.AimCentre = OutAim.GunsCentre + OutAim.GunLineDirection * OutAim.SlantDistance;
	// A round coming in at angle a covers 1/sin(a) as much ground along its path as across it.
	OutAim.AlongTrackStretch = FMath::Clamp(1.f / FMath::Max(DescentPerCm, UE_KINDA_SMALL_NUMBER), 1.f,
	                                        MaxAlongTrackStretch);
	return true;
}

FVector UWeaponStateAircraftStrafeGun::ClampDirectionToNoseCone(
	const FVector& DesiredDirection,
	const FRotator& NoseRotation) const
{
	using namespace AircraftStrafeGunSettings;
	const FVector LocalDirection = NoseRotation.UnrotateVector(DesiredDirection);
	const float YawDeg = FMath::RadiansToDegrees(FMath::Atan2(LocalDirection.Y, LocalDirection.X));
	const float PitchDeg = FMath::RadiansToDegrees(
		FMath::Atan2(LocalDirection.Z, FVector2D(LocalDirection.X, LocalDirection.Y).Size()));
	const FRotator ClampedLocal(
		FMath::Clamp(PitchDeg, -MaxDepressionBelowNoseDeg, MaxElevationAboveNoseDeg),
		FMath::Clamp(YawDeg, -MaxYawFromNoseDeg, MaxYawFromNoseDeg),
		0.f);
	return NoseRotation.RotateVector(ClampedLocal.Vector());
}

FVector UWeaponStateAircraftStrafeGun::GetGunsCentre() const
{
	FVector Sum = FVector::ZeroVector;
	int32 SocketCount = 0;
	for (const FName& SocketName : M_FireSocketNames)
	{
		if (not MeshComponent->DoesSocketExist(SocketName))
		{
			continue;
		}
		Sum += MeshComponent->GetSocketLocation(SocketName);
		++SocketCount;
	}
	return SocketCount > 0 ? Sum / SocketCount : MeshComponent->GetComponentLocation();
}

FVector UWeaponStateAircraftStrafeGun::SampleBeatenZonePoint(
	const FStrafeGunAim& Aim,
	const FVector& MuzzleLocation) const
{
	using namespace AircraftStrafeGunSettings;
	const float Inaccuracy = FMath::Clamp(1.f - WeaponData.Accuracy / 100.f, 0.f, 1.f);
	const float CrossTrackSigma = CrossTrackSigmaAtReferenceCm * Inaccuracy * Aim.SlantDistance / ReferenceDistanceCm;
	const float AlongTrackSigma = CrossTrackSigma * Aim.AlongTrackStretch;
	const float WingGunOffset = FVector::DotProduct(MuzzleLocation - Aim.GunsCentre, Aim.FlightRight2D)
		* WingGunSpacingKeptOnGround;
	const float CrossTrackOffset = WingGunOffset
		+ AircraftStrafeGunPrivate::GetClampedGaussian(MaxSigmaClamp) * CrossTrackSigma;
	const float AlongTrackOffset = AircraftStrafeGunPrivate::GetClampedGaussian(MaxSigmaClamp) * AlongTrackSigma;
	return Aim.AimCentre + Aim.FlightRight2D * CrossTrackOffset + Aim.FlightForward2D * AlongTrackOffset;
}

void UWeaponStateAircraftStrafeGun::FireRoundFromSocket(const FName& SocketName, const FStrafeGunAim& Aim)
{
	const FVector LaunchLocation = GetLaunchAndForwardVectorForSocket(SocketName).Key;
	const FVector AimPoint = SampleBeatenZonePoint(Aim, LaunchLocation);
	const FVector ToAimPoint = AimPoint - LaunchLocation;
	const float DistanceToAimPoint = ToAimPoint.Size();
	if (DistanceToAimPoint <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Direction = ToAimPoint / DistanceToAimPoint;
	const FVector TraceEnd = LaunchLocation + Direction * (DistanceToAimPoint
		+ AircraftStrafeGunSettings::TraceOvershootCm);
#if RTS_WITH_MAP_TESTS
	FAircraftStrafeShotRecorder::RecordShot(this, LaunchLocation, Direction,
	                                        MeshComponent->GetOwner()->GetActorForwardVector(), Aim.AimCentre);
#endif

	FStrafeRoundInFlight Round;
	Round.LaunchLocation = LaunchLocation;
	Round.TraceEnd = TraceEnd;
	Round.AimCentre = Aim.AimCentre;
	Round.FlightForward2D = Aim.FlightForward2D;
	Round.LaunchTime = World->GetTimeSeconds();

	FCollisionQueryParams TraceParams(FName(TEXT("FireTrace_AircraftStrafe")), true, nullptr);
	TraceParams.bTraceComplex = false;
	TraceParams.bReturnPhysicalMaterial = true;
	AppendIgnoredActorsToQuery(TraceParams);

	FTraceDelegate TraceDelegate;
	TWeakObjectPtr<UWeaponStateAircraftStrafeGun> WeakThis(this);
	TraceDelegate.BindLambda([WeakThis, Round](const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum) -> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnStrafeRoundTraceComplete(TraceDatum, Round);
	});
	World->AsyncLineTraceByChannel(
		EAsyncTraceType::Single,
		LaunchLocation,
		TraceEnd,
		TraceChannel,
		TraceParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate);
}

void UWeaponStateAircraftStrafeGun::OnStrafeRoundTraceComplete(
	const FTraceDatum& TraceDatum,
	const FStrafeRoundInFlight& Round)
{
	FStrafeRoundInFlight ResolvedRound = Round;
	ResolvedRound.Hits = TraceDatum.OutHits;
	const FVector TracerEnd = ResolvedRound.Hits.Num() > 0
		                          ? FVector(ResolvedRound.Hits[0].ImpactPoint)
		                          : ResolvedRound.TraceEnd;
	// The tracer starts at once; impact and damage wait until it gets there.
	FireTracerProjectile(ResolvedRound.LaunchLocation, TracerEnd, ResolvedRound.LaunchTime);
	ResolvedRound.ArrivalTime = ResolvedRound.LaunchTime + FVector::Dist(ResolvedRound.LaunchLocation, TracerEnd)
		/ static_cast<float>(DeveloperSettings::GamePlay::Projectile::SmallArmsProjectileSpeed);
	QueueRoundArrival(MoveTemp(ResolvedRound));
}

void UWeaponStateAircraftStrafeGun::QueueRoundArrival(FStrafeRoundInFlight&& Round)
{
	if (M_RoundsInFlight.Num() >= AircraftStrafeGunSettings::MaxRoundsInFlight)
	{
		ApplyRoundImpact(Round);
		return;
	}
	M_RoundsInFlight.Add(MoveTemp(Round));
	StartArrivalTimerIfNeeded();
}

void UWeaponStateAircraftStrafeGun::StartArrivalTimerIfNeeded()
{
	if (not IsValid(World) || M_RoundsInFlight.IsEmpty())
	{
		return;
	}
	FTimerManager& TimerManager = World->GetTimerManager();
	if (TimerManager.IsTimerActive(M_ArrivalTimerHandle))
	{
		return;
	}
	TWeakObjectPtr<UWeaponStateAircraftStrafeGun> WeakThis(this);
	FTimerDelegate ArrivalDelegate;
	ArrivalDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->ApplyArrivedRounds();
	});
	// Not looping: a destroyed weapon leaves no timer behind; ApplyArrivedRounds re-arms while rounds remain.
	TimerManager.SetTimer(M_ArrivalTimerHandle, ArrivalDelegate,
	                      AircraftStrafeGunSettings::ArrivalCheckIntervalSeconds, false);
}

void UWeaponStateAircraftStrafeGun::ApplyArrivedRounds()
{
	if (not IsValid(World))
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	// Taken out first: applying damage can run kill callbacks that must not see a half-edited queue.
	TArray<FStrafeRoundInFlight> ArrivedRounds;
	for (int32 RoundIndex = M_RoundsInFlight.Num() - 1; RoundIndex >= 0; --RoundIndex)
	{
		if (M_RoundsInFlight[RoundIndex].ArrivalTime > Now)
		{
			continue;
		}
		ArrivedRounds.Add(MoveTemp(M_RoundsInFlight[RoundIndex]));
		M_RoundsInFlight.RemoveAtSwap(RoundIndex);
	}
	for (const FStrafeRoundInFlight& ArrivedRound : ArrivedRounds)
	{
		ApplyRoundImpact(ArrivedRound);
	}
	StartArrivalTimerIfNeeded();
}

void UWeaponStateAircraftStrafeGun::ApplyRoundImpact(const FStrafeRoundInFlight& Round)
{
	const bool bHitSomething = Round.Hits.Num() > 0;
#if RTS_WITH_MAP_TESTS
	if (IsValid(World))
	{
		FAircraftStrafeShotRecorder::RecordImpact(
			this, bHitSomething, Round.LaunchLocation,
			bHitSomething ? FVector(Round.Hits[0].ImpactPoint) : Round.TraceEnd,
			Round.AimCentre, Round.FlightForward2D, Round.LaunchTime, World->GetTimeSeconds());
	}
#endif
	if (not bHitSomething)
	{
		// A round that flew past everything leaves no puff in the sky.
		return;
	}
	FTraceDatum ResolvedTrace;
	ResolvedTrace.OutHits = Round.Hits;
	OnAsyncTraceComplete(ResolvedTrace, Round.LaunchTime, Round.LaunchLocation, Round.TraceEnd);
}
