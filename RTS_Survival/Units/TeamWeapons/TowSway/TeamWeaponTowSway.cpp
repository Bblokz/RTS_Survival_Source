// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "TeamWeaponTowSway.h"

#include "Components/SkeletalMeshComponent.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"

namespace TeamWeaponTowSwayStatics
{
	// Springs are stepped at least this often; a coarse frame would otherwise make a stiff bounce explode.
	constexpr float MaxSubstepSeconds = 1.f / 120.f;
	constexpr int32 MaxSubsteps = 16;
	// Hook acceleration is low-passed with this time constant before it excites the sway.
	constexpr float AccelerationSmoothingSeconds = 0.15f;
	constexpr float MaxAngularSpeedDegreesPerSec = 360.f;
	constexpr float CentimetresPerMetre = 100.f;
	constexpr float MinDrawbarLengthCm = 10.f;
	constexpr float MinWheelHalfTrackCm = 35.f;
	constexpr float MaxWheelHalfTrackCm = 150.f;
	constexpr float WheelHalfTrackFromBoundsFraction = 0.6f;
	// A bump may throw the gun up by a few times the tyre compression before it comes down again.
	constexpr float MaxHopToCompressionRatio = 3.f;
	// Ground traces start this far above the higher of hook and wheels: at a crest the rigid pose dips the wheels
	// into the hill, and a trace starting inside the terrain finds nothing.
	constexpr float GroundProbeClearanceCm = 300.f;

	float GetAngularFrequency(const float FrequencyHz)
	{
		return 2.f * PI * FrequencyHz;
	}
}

void FTeamWeaponTowSwaySpring::Step(const float Target, const float AngularFrequency, const float DampingRatio,
                                    const float Excitation, const float DeltaSeconds)
{
	using namespace TeamWeaponTowSwayStatics;
	const float Acceleration = AngularFrequency * AngularFrequency * (Target - Angle)
		- 2.f * DampingRatio * AngularFrequency * Velocity + Excitation;
	// Semi-implicit Euler stays stable for these frequencies at the substep size.
	Velocity = FMath::Clamp(Velocity + Acceleration * DeltaSeconds, -MaxAngularSpeedDegreesPerSec,
	                        MaxAngularSpeedDegreesPerSec);
	Angle += Velocity * DeltaSeconds;
}

void FTeamWeaponTowSwaySpring::KeepInContactBand(const float ContactAngle, const float MaxBelowContact,
                                                 const float MaxAboveContact, const float Restitution)
{
	if (Angle < ContactAngle - MaxBelowContact)
	{
		Angle = ContactAngle - MaxBelowContact;
		Velocity = Velocity < 0.f ? -Velocity * Restitution : Velocity;
		return;
	}
	if (Angle > ContactAngle + MaxAboveContact)
	{
		Angle = ContactAngle + MaxAboveContact;
		Velocity = Velocity > 0.f ? -Velocity * Restitution : Velocity;
	}
}

void FTeamWeaponTowSwaySimulation::Reset()
{
	bM_IsInitialised = false;
	M_RestRelativeTransform = FTransform::Identity;
	M_SocketRotationInTruckSpace = FQuat::Identity;
	M_TrailLengthCm = 0.f;
	M_SettledPitchDegrees = 0.f;
	M_AxleLocation = FVector::ZeroVector;
	M_LastHitchLocation = FVector::ZeroVector;
	M_HitchVelocity = FVector::ZeroVector;
	M_HitchAcceleration = FVector::ZeroVector;
	M_WheelHalfTrackCm = 0.f;
	M_Yaw.Reset();
	M_Pitch.Reset();
	M_Roll.Reset();
}

bool FTeamWeaponTowSwaySimulation::TryInitialise(const FTeamWeaponTowSwaySettings& Settings, ATeamWeapon& Weapon)
{
	using namespace TeamWeaponTowSwayStatics;
	const USceneComponent* WeaponRoot = Weapon.GetRootComponent();
	const USceneComponent* AttachParent = IsValid(WeaponRoot) ? WeaponRoot->GetAttachParent() : nullptr;
	const AActor* Truck = Weapon.GetAttachParentActor();
	if (not IsValid(AttachParent) || not IsValid(Truck))
	{
		return false;
	}
	const FTransform SocketTransform = AttachParent->GetSocketTransform(WeaponRoot->GetAttachSocketName());
	M_SocketRotationInTruckSpace = Truck->GetActorQuat().Inverse() * SocketTransform.GetRotation();
	// The truck sets the attach offset and rotation when it hooks the gun; that is the pose the sway starts from.
	M_RestRelativeTransform = Weapon.GetActorTransform().GetRelativeTransform(GetHitchFrame(Weapon));
	const USkeletalMeshComponent* Mesh = Weapon.FindComponentByClass<USkeletalMeshComponent>();
	if (IsValid(Mesh))
	{
		const FBoxSphereBounds LocalBounds = Mesh->CalcBounds(FTransform::Identity);
		M_TrailLengthCm = FMath::Abs(LocalBounds.Origin.X - LocalBounds.BoxExtent.X) * Settings.TrailLengthFraction;
	}
	const FTransform HitchFrame = GetHitchFrame(Weapon);
	const FVector RestDrawbar = GetRestDrawbar(Settings, HitchFrame);
	M_AxleLocation = HitchFrame.GetLocation() + RestDrawbar;
	M_LastHitchLocation = SocketTransform.GetLocation();
	M_HitchVelocity = FVector::ZeroVector;
	M_HitchAcceleration = FVector::ZeroVector;
	const FVector2D FootprintHalfExtent = Weapon.GetFootprintHalfExtent();
	M_WheelHalfTrackCm = FMath::Clamp(FootprintHalfExtent.Y * WheelHalfTrackFromBoundsFraction,
	                                  MinWheelHalfTrackCm, MaxWheelHalfTrackCm);
	M_Yaw.Reset();
	M_Pitch.Reset();
	M_Roll.Reset();
	// Start with the wheels already on the ground instead of dropping the gun onto them.
	M_SettledPitchDegrees = 0.f;
	const float DrawbarLengthCm = RestDrawbar.Size();
	const FVector ProbeLocation(M_AxleLocation.X, M_AxleLocation.Y,
	                            FMath::Max(M_AxleLocation.Z, HitchFrame.GetLocation().Z) + GroundProbeClearanceCm);
	float AxleGroundZ = 0.f;
	if (DrawbarLengthCm > MinDrawbarLengthCm && Weapon.TryGetGroundHeightAt(ProbeLocation, AxleGroundZ))
	{
		M_SettledPitchDegrees = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(
			(AxleGroundZ - M_AxleLocation.Z) / DrawbarLengthCm, -1.f, 1.f)));
		M_Pitch.Angle = M_SettledPitchDegrees;
	}
	bM_IsInitialised = true;
	return true;
}

void FTeamWeaponTowSwaySimulation::Tick(const FTeamWeaponTowSwaySettings& Settings, ATeamWeapon& Weapon,
                                        const float DeltaSeconds)
{
	using namespace TeamWeaponTowSwayStatics;
	if (not Settings.bEnableTowSway || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	if (not bM_IsInitialised && not TryInitialise(Settings, Weapon))
	{
		return;
	}
	const USceneComponent* WeaponRoot = Weapon.GetRootComponent();
	const USceneComponent* AttachParent = IsValid(WeaponRoot) ? WeaponRoot->GetAttachParent() : nullptr;
	if (not IsValid(AttachParent))
	{
		Reset();
		return;
	}
	if (Settings.WheelHalfTrackCm > 0.f)
	{
		M_WheelHalfTrackCm = Settings.WheelHalfTrackCm;
	}
	const FTransform HitchFrame = GetHitchFrame(Weapon);
	StepSimulation(Settings, Weapon, HitchFrame, DeltaSeconds);

	const FTransform RestWorldTransform = M_RestRelativeTransform * HitchFrame;
	const FVector HitchLocation = HitchFrame.GetLocation();
	const FVector RestDrawbar = GetRestDrawbar(Settings, HitchFrame);
	const FQuat SwayRotation = GetSwayRotation(RestDrawbar);
	Weapon.SetActorLocationAndRotation(
		HitchLocation + SwayRotation.RotateVector(RestDrawbar),
		SwayRotation * RestWorldTransform.GetRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

FTransform FTeamWeaponTowSwaySimulation::GetHitchFrame(const ATeamWeapon& Weapon) const
{
	const USceneComponent* WeaponRoot = Weapon.GetRootComponent();
	const USceneComponent* AttachParent = IsValid(WeaponRoot) ? WeaponRoot->GetAttachParent() : nullptr;
	const AActor* Truck = Weapon.GetAttachParentActor();
	if (not IsValid(AttachParent) || not IsValid(Truck))
	{
		return Weapon.GetActorTransform();
	}
	const FVector HitchLocation = AttachParent->GetSocketLocation(WeaponRoot->GetAttachSocketName());
	return FTransform(Truck->GetActorQuat() * M_SocketRotationInTruckSpace, HitchLocation);
}

FVector FTeamWeaponTowSwaySimulation::GetRestDrawbar(const FTeamWeaponTowSwaySettings& Settings,
                                                     const FTransform& HitchFrame) const
{
	// In the hitch frame the attach offset is constant; only the truck's pose turns it in the world.
	const FVector AttachedLocalDrawbar = M_RestRelativeTransform.GetLocation();
	FVector LocalDrawbar = AttachedLocalDrawbar;
	// Only a gun that trails behind its hook has its trail towards the truck.
	const FVector HitchInWeaponSpace = M_RestRelativeTransform.InverseTransformPosition(FVector::ZeroVector);
	const FVector PlanarLocalDrawbar(AttachedLocalDrawbar.X, AttachedLocalDrawbar.Y, 0.f);
	const float PlanarLengthCm = PlanarLocalDrawbar.Size();
	const bool bShouldLengthen = Settings.bPlaceTrailEndOnHook && M_TrailLengthCm > PlanarLengthCm &&
		HitchInWeaponSpace.X < 0.f && PlanarLengthCm > KINDA_SMALL_NUMBER;
	if (bShouldLengthen)
	{
		LocalDrawbar = PlanarLocalDrawbar * (M_TrailLengthCm / PlanarLengthCm) +
			FVector(0.f, 0.f, AttachedLocalDrawbar.Z);
	}
	return HitchFrame.TransformVector(LocalDrawbar);
}

void FTeamWeaponTowSwaySimulation::StepSimulation(const FTeamWeaponTowSwaySettings& Settings, const ATeamWeapon& Weapon,
                                                  const FTransform& HitchFrame, const float DeltaSeconds)
{
	using namespace TeamWeaponTowSwayStatics;
	const FVector HitchLocation = HitchFrame.GetLocation();
	const FTransform RestWorldTransform = M_RestRelativeTransform * HitchFrame;
	const FVector RestDrawbar = GetRestDrawbar(Settings, HitchFrame);
	const FVector RestPlanarDirection = FVector(RestDrawbar.X, RestDrawbar.Y, 0.f).GetSafeNormal();
	if (RestPlanarDirection.IsNearlyZero())
	{
		return;
	}

	const float HitchTravelTowardsGunCm = UpdateHitchMotion(HitchLocation, RestPlanarDirection, DeltaSeconds);
	const float YawTarget = GetTrailerYawTarget(Settings, HitchLocation, RestPlanarDirection, HitchTravelTowardsGunCm);
	const FTeamWeaponTowSwayGroundTargets Targets = GetGroundTargets(Settings, Weapon, HitchLocation, RestWorldTransform,
	                                                                 RestDrawbar);
	StepSprings(Settings, Targets, YawTarget, DeltaSeconds);

	// The dragged axle sits where the displayed swing puts it, so spring lag feeds back into the trailer path.
	const FVector DisplayedPlanarDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(M_Yaw.Angle))
		.RotateVector(RestPlanarDirection);
	const float PlanarLengthCm = FVector(RestDrawbar.X, RestDrawbar.Y, 0.f).Size();
	M_AxleLocation = HitchLocation + DisplayedPlanarDirection * PlanarLengthCm;
}

float FTeamWeaponTowSwaySimulation::UpdateHitchMotion(const FVector& HitchLocation, const FVector& RestPlanarDirection,
                                                      const float DeltaSeconds)
{
	using namespace TeamWeaponTowSwayStatics;
	// Hook motion this frame drives the trailer kinematics and, low-passed, the inertial kicks.
	const FVector HitchVelocity = (HitchLocation - M_LastHitchLocation) / DeltaSeconds;
	const FVector RawAcceleration = (HitchVelocity - M_HitchVelocity) / DeltaSeconds;
	const float SmoothingAlpha = 1.f - FMath::Exp(-DeltaSeconds / AccelerationSmoothingSeconds);
	M_HitchAcceleration = FMath::Lerp(M_HitchAcceleration, RawAcceleration, SmoothingAlpha);
	const float HitchTravelTowardsGunCm = FVector::DotProduct(HitchLocation - M_LastHitchLocation, RestPlanarDirection);
	M_HitchVelocity = HitchVelocity;
	M_LastHitchLocation = HitchLocation;
	return HitchTravelTowardsGunCm;
}

FTeamWeaponTowSwayGroundTargets FTeamWeaponTowSwaySimulation::GetGroundTargets(
	const FTeamWeaponTowSwaySettings& Settings, const ATeamWeapon& Weapon, const FVector& HitchLocation,
	const FTransform& RestWorldTransform, const FVector& RestDrawbar) const
{
	using namespace TeamWeaponTowSwayStatics;
	// Ground under the swung gun: the origin is where the tyres touch the ground.
	const FQuat YawRotation(FVector::UpVector, FMath::DegreesToRadians(M_Yaw.Angle));
	const FVector SwungDrawbar = YawRotation.RotateVector(RestDrawbar);
	FTeamWeaponTowSwayGroundTargets Targets;
	Targets.SwungPlanarDirection = FVector(SwungDrawbar.X, SwungDrawbar.Y, 0.f).GetSafeNormal();
	Targets.LateralAxis = FVector::CrossProduct(Targets.SwungPlanarDirection, FVector::UpVector);
	Targets.DrawbarLengthCm = FMath::Max(SwungDrawbar.Size(), MinDrawbarLengthCm);
	const FVector RigidAxleLocation = HitchLocation + SwungDrawbar;
	float PitchTarget = M_Pitch.Angle;
	float RollTarget = M_Roll.Angle;
	const float GroundProbeZ = FMath::Max(RigidAxleLocation.Z, HitchLocation.Z) + GroundProbeClearanceCm;
	const auto GetGroundProbe = [GroundProbeZ](const FVector& Location)
	{
		return FVector(Location.X, Location.Y, GroundProbeZ);
	};
	float AxleGroundZ = 0.f;
	if (Weapon.TryGetGroundHeightAt(GetGroundProbe(RigidAxleLocation), AxleGroundZ))
	{
		const float HeightError = AxleGroundZ - RigidAxleLocation.Z;
		PitchTarget = FMath::RadiansToDegrees(FMath::Asin(
			FMath::Clamp(HeightError / Targets.DrawbarLengthCm, -1.f, 1.f)));
	}
	// Roll is relative to the rigid pose, which already leans with the truck: only the difference is applied.
	const FVector GunRight = YawRotation.RotateVector(RestWorldTransform.GetRotation().GetRightVector());
	const FVector RightWheelLocation = RigidAxleLocation + GunRight * M_WheelHalfTrackCm;
	const FVector LeftWheelLocation = RigidAxleLocation - GunRight * M_WheelHalfTrackCm;
	float LeftGroundZ = 0.f;
	float RightGroundZ = 0.f;
	if (Weapon.TryGetGroundHeightAt(GetGroundProbe(LeftWheelLocation), LeftGroundZ) &&
		Weapon.TryGetGroundHeightAt(GetGroundProbe(RightWheelLocation), RightGroundZ))
	{
		const float RigidHeightDifference = RightWheelLocation.Z - LeftWheelLocation.Z;
		const float GroundHeightDifference = RightGroundZ - LeftGroundZ;
		// Positive roll lowers the side along the lateral axis; the gun's right may point either way along it.
		const float RightSideSign = FVector::DotProduct(GunRight, Targets.LateralAxis) >= 0.f ? 1.f : -1.f;
		const float RollSine = RightSideSign * (RigidHeightDifference - GroundHeightDifference) /
			(2.f * M_WheelHalfTrackCm);
		RollTarget = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(RollSine, -1.f, 1.f)));
	}
	Targets.PitchDegrees = FMath::Clamp(PitchTarget, M_SettledPitchDegrees - Settings.MaxPitchDegrees,
	                                    M_SettledPitchDegrees + Settings.MaxPitchDegrees);
	Targets.RollDegrees = FMath::Clamp(RollTarget, -Settings.MaxRollDegrees, Settings.MaxRollDegrees);
	return Targets;
}

void FTeamWeaponTowSwaySimulation::StepSprings(const FTeamWeaponTowSwaySettings& Settings,
                                               const FTeamWeaponTowSwayGroundTargets& Targets, const float YawTarget,
                                               const float DeltaSeconds)
{
	using namespace TeamWeaponTowSwayStatics;
	const float LongitudinalAcceleration = FVector::DotProduct(M_HitchAcceleration, -Targets.SwungPlanarDirection) /
		CentimetresPerMetre;
	const float LateralAcceleration = FVector::DotProduct(M_HitchAcceleration, Targets.LateralAxis) /
		CentimetresPerMetre;
	const float YawFrequency = GetAngularFrequency(Settings.YawFrequencyHz);
	const float BounceFrequency = GetAngularFrequency(Settings.BounceFrequencyHz);
	const float BounceStiffness = BounceFrequency * BounceFrequency;
	// Expressed as the offset the spring would settle at under a constant acceleration.
	const float PitchExcitation = BounceStiffness * Settings.PitchNodDegreesPerAcceleration * LongitudinalAcceleration;
	const float RollExcitation = -BounceStiffness * Settings.RollLeanDegreesPerAcceleration * LateralAcceleration;

	// Tyres press in by at most their compression; lifting off is allowed but limited to a short hop.
	const float CompressionPitchDegrees = FMath::RadiansToDegrees(
		FMath::Asin(FMath::Clamp(Settings.TyreCompressionCm / Targets.DrawbarLengthCm, 0.f, 1.f)));
	const float CompressionRollDegrees = FMath::RadiansToDegrees(
		FMath::Asin(FMath::Clamp(Settings.TyreCompressionCm / M_WheelHalfTrackCm, 0.f, 1.f)));
	const float MaxHopPitchDegrees = CompressionPitchDegrees * MaxHopToCompressionRatio;
	const int32 Substeps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / MaxSubstepSeconds), 1, MaxSubsteps);
	const float SubstepSeconds = DeltaSeconds / Substeps;
	for (int32 Substep = 0; Substep < Substeps; ++Substep)
	{
		M_Yaw.Step(YawTarget, YawFrequency, Settings.YawDampingRatio, 0.f, SubstepSeconds);
		M_Pitch.Step(Targets.PitchDegrees, BounceFrequency, Settings.BounceDampingRatio, PitchExcitation,
		             SubstepSeconds);
		M_Pitch.KeepInContactBand(Targets.PitchDegrees, CompressionPitchDegrees, MaxHopPitchDegrees,
		                          Settings.GroundBounceRestitution);
		M_Roll.Step(Targets.RollDegrees, BounceFrequency, Settings.BounceDampingRatio, RollExcitation, SubstepSeconds);
		// Any roll away from the ground presses one tyre in, so roll keeps to the compression band both ways.
		M_Roll.KeepInContactBand(Targets.RollDegrees, CompressionRollDegrees, CompressionRollDegrees,
		                         Settings.GroundBounceRestitution);
	}
	M_Yaw.Angle = FMath::Clamp(M_Yaw.Angle, -Settings.MaxYawDegrees, Settings.MaxYawDegrees);
	M_Pitch.Angle = FMath::Clamp(M_Pitch.Angle, M_SettledPitchDegrees - Settings.MaxPitchDegrees,
	                             M_SettledPitchDegrees + Settings.MaxPitchDegrees);
	M_Roll.Angle = FMath::Clamp(M_Roll.Angle, -Settings.MaxRollDegrees, Settings.MaxRollDegrees);
}

float FTeamWeaponTowSwaySimulation::GetTrailerYawTarget(const FTeamWeaponTowSwaySettings& Settings,
                                                        const FVector& HitchLocation,
                                                        const FVector& RestDrawbarPlanarDirection,
                                                        const float HitchTravelTowardsGunCm)
{
	using namespace TeamWeaponTowSwayStatics;
	// Follow the leader: the axle is pulled straight towards the hook, which swings it into every turn.
	const FVector PlanarToAxle = FVector(M_AxleLocation.X - HitchLocation.X, M_AxleLocation.Y - HitchLocation.Y, 0.f);
	if (PlanarToAxle.SizeSquared() < FMath::Square(MinDrawbarLengthCm))
	{
		return 0.f;
	}
	const FVector TrailingDirection = PlanarToAxle.GetSafeNormal();
	const float CrossZ = FVector::CrossProduct(RestDrawbarPlanarDirection, TrailingDirection).Z;
	const float Dot = FVector::DotProduct(RestDrawbarPlanarDirection, TrailingDirection);
	float YawTarget = FMath::RadiansToDegrees(FMath::Atan2(CrossZ, Dot));
	// Reversing pushes the axle and would jackknife the gun; straighten it with the distance backed up instead.
	if (HitchTravelTowardsGunCm > 0.f)
	{
		const float Straightening = FMath::Clamp(
			Settings.ReverseStraighteningPerMeter * HitchTravelTowardsGunCm / CentimetresPerMetre, 0.f, 1.f);
		YawTarget *= 1.f - Straightening;
	}
	return FMath::Clamp(YawTarget, -Settings.MaxYawDegrees, Settings.MaxYawDegrees);
}

FQuat FTeamWeaponTowSwaySimulation::GetSwayRotation(const FVector& RestDrawbar) const
{
	// Yaw about the vertical through the hook, then pitch the drawbar up or down, then roll about the drawbar.
	const FQuat YawRotation(FVector::UpVector, FMath::DegreesToRadians(M_Yaw.Angle));
	const FVector SwungDrawbar = YawRotation.RotateVector(RestDrawbar);
	const FVector SwungPlanarDirection = FVector(SwungDrawbar.X, SwungDrawbar.Y, 0.f).GetSafeNormal();
	// Positive pitch about this axis lifts the far end of the drawbar.
	const FVector PitchAxis = FVector::CrossProduct(SwungPlanarDirection, FVector::UpVector).GetSafeNormal();
	const FQuat PitchRotation(PitchAxis, FMath::DegreesToRadians(M_Pitch.Angle));
	const FVector RollAxis = (PitchRotation * YawRotation).RotateVector(RestDrawbar).GetSafeNormal();
	// Positive roll lowers the right wheel, matching the sign of the roll target.
	const FQuat RollRotation(RollAxis, FMath::DegreesToRadians(M_Roll.Angle));
	return RollRotation * PitchRotation * YawRotation;
}
