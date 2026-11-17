// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WeaponStateAircraftStrafeGun.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "LandscapeProxy.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Physics/FRTS_PhysicsHelper.h"
#include "RTS_Survival/Physics/RTSSurfaceSubtypes.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Weapons/AircraftWeapon/AircraftStrafeGun/AircraftStrafeShotRecorder.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
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

	ECollisionChannel GetVictimObjectType(const int32 OwningPlayer)
	{
		return OwningPlayer == 1 ? COLLISION_OBJ_ENEMY : COLLISION_OBJ_PLAYER;
	}

	float GetTracerFlightSeconds(const FVector& From, const FVector& To)
	{
		return FVector::Dist(From, To) / static_cast<float>(DeveloperSettings::GamePlay::Projectile::SmallArmsProjectileSpeed);
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
	FStrafeVolley Volley;
	PlanVolley(Aim, Volley);
	if (not Aim.bReachesGround)
	{
		// The gun line ends in the air: tracers only, nothing to hit or draw.
		ResolveVolley(Volley, nullptr);
		return;
	}
	RequestGroundProbe(Volley);
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
	const float Inaccuracy = FMath::Clamp(1.f - WeaponData.Accuracy / 100.f, 0.f, 1.f);
	OutAim.CrossTrackSigma = FMath::Max(
		MinCrossTrackSigmaCm, CrossTrackSigmaAtReferenceCm * Inaccuracy * OutAim.SlantDistance / ReferenceDistanceCm);
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
	const float AlongTrackSigma = Aim.CrossTrackSigma * Aim.AlongTrackStretch;
	const float WingGunOffset = FVector::DotProduct(MuzzleLocation - Aim.GunsCentre, Aim.FlightRight2D)
		* WingGunSpacingKeptOnGround;
	const float CrossTrackOffset = WingGunOffset
		+ AircraftStrafeGunPrivate::GetClampedGaussian(MaxSigmaClamp) * Aim.CrossTrackSigma;
	const float AlongTrackOffset = AircraftStrafeGunPrivate::GetClampedGaussian(MaxSigmaClamp) * AlongTrackSigma;
	return Aim.AimCentre + Aim.FlightRight2D * CrossTrackOffset + Aim.FlightForward2D * AlongTrackOffset;
}

void UWeaponStateAircraftStrafeGun::PlanVolley(const FStrafeGunAim& Aim, FStrafeVolley& OutVolley)
{
	OutVolley.Aim = Aim;
	OutVolley.LaunchTime = World->GetTimeSeconds();
	OutVolley.Rounds.Reserve(M_FireSocketNames.Num());
	for (const FName& SocketName : M_FireSocketNames)
	{
		FStrafeRoundPlan Round;
		Round.LaunchLocation = GetLaunchAndForwardVectorForSocket(SocketName).Key;
		Round.AimPoint = SampleBeatenZonePoint(Aim, Round.LaunchLocation);
		OutVolley.Rounds.Add(Round);
	}
}

void UWeaponStateAircraftStrafeGun::RequestGroundProbe(const FStrafeVolley& Volley)
{
	using namespace AircraftStrafeGunSettings;
	const FVector ProbeStart = Volley.Aim.AimCentre + FVector::UpVector * GroundProbeAboveCm;
	const FVector ProbeEnd = Volley.Aim.AimCentre - FVector::UpVector * GroundProbeBelowCm;
	FCollisionQueryParams ProbeParams(FName(TEXT("AircraftStrafeGroundProbe")), false, nullptr);
	ProbeParams.bReturnPhysicalMaterial = true;
	AppendIgnoredActorsToQuery(ProbeParams);
	FTraceDelegate ProbeDelegate;
	TWeakObjectPtr<UWeaponStateAircraftStrafeGun> WeakThis(this);
	ProbeDelegate.BindLambda([WeakThis, Volley](const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum) -> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnGroundProbeComplete(TraceDatum, Volley);
	});
	// Landscape and static scenery only: the probe must find the ground under the units, not a unit's roof.
	World->AsyncLineTraceByObjectType(EAsyncTraceType::Single, ProbeStart, ProbeEnd,
	                                  FCollisionObjectQueryParams(ECC_WorldStatic), ProbeParams, &ProbeDelegate);
}

void UWeaponStateAircraftStrafeGun::OnGroundProbeComplete(const FTraceDatum& TraceDatum, const FStrafeVolley& Volley)
{
	FStrafeGroundPlane Ground;
	Ground.Point = Volley.Aim.AimCentre;
#if RTS_WITH_MAP_TESTS
	FAircraftStrafeShotRecorder::RecordGroundProbe(TraceDatum.OutHits.Num() > 0);
#endif
	if (TraceDatum.OutHits.Num() > 0)
	{
		const FHitResult& GroundHit = TraceDatum.OutHits[0];
		Ground.Point = GroundHit.ImpactPoint;
		Ground.Normal = GroundHit.ImpactNormal;
		Ground.PhysicalMaterial = GroundHit.PhysMaterial;
		Ground.Surface = UAircraftStrafeImpactBatcher::GetSurfaceForPhysicalSurface(
			FRTS_PhysicsHelper::GetRTSSurfaceType(GroundHit.PhysMaterial));
		Ground.Landscape = Cast<ALandscapeProxy>(GroundHit.GetActor());
	}
	ResolveVolley(Volley, &Ground);
}

FVector UWeaponStateAircraftStrafeGun::ProjectOntoGround(const FVector& Point, const FStrafeGroundPlane& Ground) const
{
	if (ALandscapeProxy* Landscape = Ground.Landscape.Get())
	{
		const TOptional<float> SurfaceHeight = Landscape->GetHeightAtLocation(Point);
		if (SurfaceHeight.IsSet())
		{
			return FVector(Point.X, Point.Y, SurfaceHeight.GetValue());
		}
	}
	const float NormalZ = FMath::Max(Ground.Normal.Z, AircraftStrafeGunSettings::MinGroundNormalZ);
	const float HeightOnPlane = Ground.Point.Z - (Ground.Normal.X * (Point.X - Ground.Point.X)
		+ Ground.Normal.Y * (Point.Y - Ground.Point.Y)) / NormalZ;
	return FVector(Point.X, Point.Y, HeightOnPlane);
}

void UWeaponStateAircraftStrafeGun::ResolveVolley(const FStrafeVolley& Volley, const FStrafeGroundPlane* Ground)
{
	if (not IsValid(World))
	{
		return;
	}
	const TArray<FStrafeUnitCandidate> Candidates = Ground != nullptr
		                                                ? FindUnitsUnderVolley(Volley, *Ground)
		                                                : TArray<FStrafeUnitCandidate>();
	for (const FStrafeRoundPlan& RoundPlan : Volley.Rounds)
	{
		FStrafeRoundInFlight Round;
		Round.LaunchLocation = RoundPlan.LaunchLocation;
		Round.AimCentre = Volley.Aim.AimCentre;
		Round.FlightForward2D = Volley.Aim.FlightForward2D;
		Round.LaunchTime = Volley.LaunchTime;
		Round.ImpactLocation = RoundPlan.AimPoint + (RoundPlan.AimPoint - RoundPlan.LaunchLocation).GetSafeNormal()
			* AircraftStrafeGunSettings::TraceOvershootCm;
		if (Ground != nullptr)
		{
			const FVector GroundPoint = ProjectOntoGround(RoundPlan.AimPoint, *Ground);
			Round.bHasImpact = true;
			Round.ImpactLocation = GroundPoint;
			Round.ImpactNormal = Ground->Normal;
			Round.Surface = Ground->Surface;
			Round.GroundMaterial = Ground->PhysicalMaterial;
			const FStrafeUnitCandidate* Unit = FindUnitUnderPoint(Candidates, GroundPoint);
			FHitResult UnitHit;
			if (Unit != nullptr && Unit->Actor.IsValid() && TraceRoundIntoUnit(RoundPlan, GroundPoint, *Unit->Actor, UnitHit))
			{
				Round.UnitHits.Add(UnitHit);
				Round.ImpactLocation = UnitHit.ImpactPoint;
				Round.ImpactNormal = UnitHit.ImpactNormal;
			}
			AddExtraSpurts(Volley, *Ground, Round);
		}
#if RTS_WITH_MAP_TESTS
		FAircraftStrafeShotRecorder::RecordShot(this, Round.LaunchLocation, Round.ImpactLocation - Round.LaunchLocation,
		                                        MeshComponent->GetOwner()->GetActorForwardVector(), Round.AimCentre);
#endif
		// The tracer starts at once; impact and damage wait until it gets there.
		FireTracerProjectile(Round.LaunchLocation, Round.ImpactLocation, Round.LaunchTime);
		Round.ArrivalTime = Round.LaunchTime
			+ AircraftStrafeGunPrivate::GetTracerFlightSeconds(Round.LaunchLocation, Round.ImpactLocation);
		QueueRoundArrival(MoveTemp(Round));
	}
}

TArray<UWeaponStateAircraftStrafeGun::FStrafeUnitCandidate> UWeaponStateAircraftStrafeGun::FindUnitsUnderVolley(
	const FStrafeVolley& Volley,
	const FStrafeGroundPlane& Ground) const
{
	TArray<FStrafeUnitCandidate> Candidates;
	float ZoneRadius = 0.f;
	for (const FStrafeRoundPlan& Round : Volley.Rounds)
	{
		ZoneRadius = FMath::Max(ZoneRadius, FVector::Dist2D(Round.AimPoint, Ground.Point));
	}
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(AircraftStrafeGunPrivate::GetVictimObjectType(OwningPlayer));
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AircraftStrafeUnitSearch), false);
	AppendIgnoredActorsToQuery(QueryParams);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Ground.Point, FQuat::Identity, ObjectQueryParams,
	                                FCollisionShape::MakeSphere(ZoneRadius + AircraftStrafeGunSettings::UnitSearchMarginCm),
	                                QueryParams);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlapActor = Overlap.GetActor();
		const bool bIsKnown = Candidates.ContainsByPredicate([OverlapActor](const FStrafeUnitCandidate& Candidate)
		{
			return Candidate.Actor.Get() == OverlapActor;
		});
		if (not IsValid(OverlapActor) || bIsKnown)
		{
			continue;
		}
		FStrafeUnitCandidate Candidate;
		Candidate.Actor = OverlapActor;
		Candidate.ActorTransform = OverlapActor->GetActorTransform();
		Candidate.LocalBounds = OverlapActor->CalculateComponentsBoundingBoxInLocalSpace(false);
		Candidates.Add(Candidate);
	}
	return Candidates;
}

const UWeaponStateAircraftStrafeGun::FStrafeUnitCandidate* UWeaponStateAircraftStrafeGun::FindUnitUnderPoint(
	const TArray<FStrafeUnitCandidate>& Candidates,
	const FVector& GroundPoint) const
{
	for (const FStrafeUnitCandidate& Candidate : Candidates)
	{
		if (not Candidate.LocalBounds.IsValid)
		{
			continue;
		}
		const FVector LocalPoint = Candidate.ActorTransform.InverseTransformPosition(GroundPoint);
		const bool bIsInsideFootprint = LocalPoint.X >= Candidate.LocalBounds.Min.X
			&& LocalPoint.X <= Candidate.LocalBounds.Max.X
			&& LocalPoint.Y >= Candidate.LocalBounds.Min.Y
			&& LocalPoint.Y <= Candidate.LocalBounds.Max.Y;
		if (bIsInsideFootprint)
		{
			return &Candidate;
		}
	}
	return nullptr;
}

bool UWeaponStateAircraftStrafeGun::TraceRoundIntoUnit(
	const FStrafeRoundPlan& Round,
	const FVector& GroundPoint,
	const AActor& Unit,
	FHitResult& OutHit) const
{
	// Only rounds already known to land on a unit trace, and only to find its surface: the armour check needs the
	// plate, the angle and the physical material of the hit.
	const FVector Direction = (GroundPoint - Round.LaunchLocation).GetSafeNormal();
	const FVector TraceEnd = GroundPoint + Direction * AircraftStrafeGunSettings::UnitHitTraceOvershootCm;
	FCollisionQueryParams TraceParams(FName(TEXT("AircraftStrafeUnitHit")), false, nullptr);
	TraceParams.bReturnPhysicalMaterial = true;
	AppendIgnoredActorsToQuery(TraceParams);
	if (not World->LineTraceSingleByChannel(OutHit, Round.LaunchLocation, TraceEnd, TraceChannel, TraceParams))
	{
		return false;
	}
	// Only a hit on this unit (or a part attached to it, like a turret) counts; anything else in front of it, a
	// wall or the ground, takes the round and it stays a ground impact.
	for (const AActor* HitActor = OutHit.GetActor(); IsValid(HitActor); HitActor = HitActor->GetAttachParentActor())
	{
		if (HitActor == &Unit)
		{
			return true;
		}
	}
	return false;
}

void UWeaponStateAircraftStrafeGun::AddExtraSpurts(
	const FStrafeVolley& Volley,
	const FStrafeGroundPlane& Ground,
	FStrafeRoundInFlight& Round) const
{
	using namespace AircraftStrafeGunSettings;
	const float CrossSigma = Volley.Aim.CrossTrackSigma * ExtraSpurtSpreadShareOfZone;
	const float AlongSigma = CrossSigma * Volley.Aim.AlongTrackStretch;
	const FVector GroundImpact = ProjectOntoGround(Round.ImpactLocation, Ground);
	Round.ExtraSpurts.Reserve(ExtraSpurtsPerRound);
	for (int32 SpurtIndex = 0; SpurtIndex < ExtraSpurtsPerRound; ++SpurtIndex)
	{
		const FVector Offset = Volley.Aim.FlightRight2D * AircraftStrafeGunPrivate::GetClampedGaussian(MaxSigmaClamp) * CrossSigma
			+ Volley.Aim.FlightForward2D * AircraftStrafeGunPrivate::GetClampedGaussian(MaxSigmaClamp) * AlongSigma;
		Round.ExtraSpurts.Add(ProjectOntoGround(GroundImpact + Offset, Ground));
	}
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
	// Looping while rounds are in the air; ApplyArrivedRounds stops it once the queue is empty. Bound to this object,
	// so the timer manager drops it if the weapon is destroyed.
	TimerManager.SetTimer(M_ArrivalTimerHandle,
	                      FTimerDelegate::CreateUObject(this, &UWeaponStateAircraftStrafeGun::ApplyArrivedRounds),
	                      AircraftStrafeGunSettings::ArrivalCheckIntervalSeconds, true);
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
	if (M_RoundsInFlight.IsEmpty())
	{
		World->GetTimerManager().ClearTimer(M_ArrivalTimerHandle);
	}
}

void UWeaponStateAircraftStrafeGun::ApplyRoundImpact(const FStrafeRoundInFlight& Round)
{
#if RTS_WITH_MAP_TESTS
	if (IsValid(World))
	{
		FAircraftStrafeShotRecorder::RecordImpact(this, Round.bHasImpact, Round.LaunchLocation, Round.ImpactLocation,
		                                          Round.AimCentre, Round.FlightForward2D, Round.LaunchTime,
		                                          World->GetTimeSeconds());
	}
#endif
	if (not Round.bHasImpact)
	{
		// A round that flew past everything leaves no puff in the sky.
		return;
	}
	if (Round.UnitHits.Num() > 0)
	{
		// The regular trace path resolves armour, damage, kills and the hit's own impact effect.
		FTraceDatum UnitTrace;
		UnitTrace.OutHits = Round.UnitHits;
		ApplyQueuedRoundThroughTracePath(UnitTrace, Round);
		if (UAircraftStrafeImpactBatcher* Batcher = GetReadyStrafeImpactBatcher())
		{
			Batcher->AddImpact(EAircraftStrafeImpactGroup::Unit, Round.ImpactLocation, Round.ImpactNormal,
			                   WeaponData.WeaponCalibre, EAircraftStrafeImpactSurface::Metal);
		}
	}
	else
	{
		DrawGroundImpact(Round);
	}
	PlayThrottledImpactSound(Round.ImpactLocation);
}

void UWeaponStateAircraftStrafeGun::ApplyQueuedRoundThroughTracePath(
	FTraceDatum& TraceDatum,
	const FStrafeRoundInFlight& Round)
{
	bM_IsApplyingQueuedRound = true;
	OnAsyncTraceComplete(TraceDatum, Round.LaunchTime, Round.LaunchLocation, Round.ImpactLocation);
	bM_IsApplyingQueuedRound = false;
}

void UWeaponStateAircraftStrafeGun::DrawGroundImpact(const FStrafeRoundInFlight& Round)
{
	UAircraftStrafeImpactBatcher* Batcher = GetReadyStrafeImpactBatcher();
	if (Batcher == nullptr)
	{
		// No strafe impact system: draw the round as a regular trace impact on the ground (no extra spurts).
		FHitResult GroundHit;
		GroundHit.bBlockingHit = true;
		GroundHit.Location = Round.ImpactLocation;
		GroundHit.ImpactPoint = Round.ImpactLocation;
		GroundHit.ImpactNormal = Round.ImpactNormal;
		GroundHit.Normal = Round.ImpactNormal;
		GroundHit.PhysMaterial = Round.GroundMaterial;
		FTraceDatum GroundTrace;
		GroundTrace.OutHits.Add(GroundHit);
		ApplyQueuedRoundThroughTracePath(GroundTrace, Round);
		return;
	}
	Batcher->AddImpact(EAircraftStrafeImpactGroup::Ground, Round.ImpactLocation, Round.ImpactNormal,
	                   WeaponData.WeaponCalibre, Round.Surface);
	if (FMath::FRand() < AircraftStrafeGunSettings::BulletPockChancePerRound)
	{
		Batcher->AddBulletPock(Round.ImpactLocation, Round.ImpactNormal, WeaponData.WeaponCalibre);
	}
	for (const FVector& ExtraSpurt : Round.ExtraSpurts)
	{
		Batcher->AddImpact(EAircraftStrafeImpactGroup::Ground, ExtraSpurt, Round.ImpactNormal,
		                   WeaponData.WeaponCalibre, Round.Surface);
	}
}

void UWeaponStateAircraftStrafeGun::PlayThrottledImpactSound(const FVector& Location)
{
	const float Now = IsValid(World) ? World->GetTimeSeconds() : 0.f;
	if (M_LastImpactSoundTime >= 0.f && Now - M_LastImpactSoundTime < AircraftStrafeGunSettings::ImpactSoundIntervalSeconds)
	{
		return;
	}
	const FRTSSurfaceImpactData* SandImpact = M_WeaponVfx.SurfaceImpactEffects.Find(ERTSSurfaceType::Sand);
	if (SandImpact == nullptr || SandImpact->ImpactSound == nullptr)
	{
		return;
	}
	M_LastImpactSoundTime = Now;
	PlayPooledImpactSound(Location, SandImpact->ImpactSound);
}

UAircraftStrafeImpactBatcher* UWeaponStateAircraftStrafeGun::GetReadyStrafeImpactBatcher() const
{
	const ASmallArmsProjectileManager* ProjectileManager = GetTraceProjectileManager();
	if (ProjectileManager == nullptr)
	{
		return nullptr;
	}
	UAircraftStrafeImpactBatcher* Batcher = ProjectileManager->GetAircraftStrafeImpactBatcher();
	return Batcher != nullptr && Batcher->GetIsReadyToDraw() ? Batcher : nullptr;
}
