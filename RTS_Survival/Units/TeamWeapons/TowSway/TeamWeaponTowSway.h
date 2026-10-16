// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "TeamWeaponTowSway.generated.h"

class ATeamWeapon;

/** Tuning of the procedural sway a towed team weapon shows behind its truck. */
USTRUCT(BlueprintType)
struct FTeamWeaponTowSwaySettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow")
	bool bEnableTowSway = true;

	// Largest angle the gun may swing out to the side of the tow hook in a turn.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.0", ClampMax = "85.0"))
	float MaxYawDegrees = 55.f;

	// Around the pitch the gun had when hooked: a gun on a different slope than its truck needs the full difference,
	// and a gun hooked on a bank starts far from level.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float MaxPitchDegrees = 45.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float MaxRollDegrees = 30.f;

	// How quickly the swing follows the trailer geometry; lower feels heavier.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.1"))
	float YawFrequencyHz = 1.6f;

	// Below 1 the swing overshoots a little before it settles.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.05"))
	float YawDampingRatio = 0.55f;

	// Bounce of the gun on its tyres when the ground under the wheels changes.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.1"))
	float BounceFrequencyHz = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.05"))
	float BounceDampingRatio = 0.35f;

	// How far a tyre may press into the ground before the bounce is stopped; the gun may hop up, not sink in.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.0"))
	float TyreCompressionCm = 4.f;

	// Share of the downward speed kept when a tyre hits the ground; above 0 the gun hops a little.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GroundBounceRestitution = 0.25f;

	// Nod of the gun when the truck pulls away or brakes, in degrees per m/s² of truck acceleration.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.0"))
	float PitchNodDegreesPerAcceleration = 0.8f;

	// Lean of the gun out of a turn, in degrees per m/s² of sideways acceleration.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.0"))
	float RollLeanDegreesPerAcceleration = 1.2f;

	// Half the distance between the wheels; 0 takes it from the mesh bounds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.0"))
	float WheelHalfTrackCm = 0.f;

	// Moves the gun back along the drawbar until the end of its trail reaches the hook, so a short attach offset
	// does not push the gun into the truck or hold its wheels off the ground.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow")
	bool bPlaceTrailEndOnHook = true;

	// Share of the mesh's rear reach taken as the trail length.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.1", ClampMax = "1.0", EditCondition = "bPlaceTrailEndOnHook"))
	float TrailLengthFraction = 0.9f;

	// Fraction of the swing taken out per metre the truck reverses, so backing up does not jackknife the gun.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Tow", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReverseStraighteningPerMeter = 0.35f;
};

/** One damped spring of the sway; angles in degrees. */
struct FTeamWeaponTowSwaySpring
{
	void Reset()
	{
		Angle = 0.f;
		Velocity = 0.f;
	}

	void Step(const float Target, const float AngularFrequency, const float DampingRatio, const float Excitation,
	          const float DeltaSeconds);

	/**
	 * @brief Keeps the angle within a band around the ground contact angle and bounces off its edges.
	 * @param ContactAngle Angle at which the tyres touch the ground.
	 * @param MaxBelowContact How far below contact (tyres pressed in) the angle may go.
	 * @param MaxAboveContact How far above contact (tyres lifted) the angle may go.
	 * @param Restitution Share of the speed kept, reversed, when an edge is hit.
	 */
	void KeepInContactBand(const float ContactAngle, const float MaxBelowContact, const float MaxAboveContact,
	                       const float Restitution);

	float Angle = 0.f;
	float Velocity = 0.f;
};

/** Where the ground wants the swung gun this frame, and the frame it is measured in. */
struct FTeamWeaponTowSwayGroundTargets
{
	float PitchDegrees = 0.f;
	float RollDegrees = 0.f;
	float DrawbarLengthCm = 0.f;
	FVector SwungPlanarDirection = FVector::ForwardVector;
	FVector LateralAxis = FVector::RightVector;
};

/**
 * @brief Visual-only trailer behaviour of a towed team weapon: the gun pivots about the tow hook, trails into turns
 * like a real trailer, follows the ground under its wheels and bounces on bumps, braking and turns.
 * The rigid tow attachment stays; only the weapon's transform relative to the hook is changed each tick.
 */
struct FTeamWeaponTowSwaySimulation
{
	void Reset();
	bool GetIsRunning() const { return bM_IsInitialised; }

	/**
	 * @brief Advances the sway one frame and moves the attached weapon about its tow hook.
	 * @param Settings Tuning of the weapon.
	 * @param Weapon The towed weapon; must be attached to the tow socket.
	 * @param DeltaSeconds Frame time.
	 */
	void Tick(const FTeamWeaponTowSwaySettings& Settings, ATeamWeapon& Weapon, const float DeltaSeconds);

private:
	bool TryInitialise(const FTeamWeaponTowSwaySettings& Settings, ATeamWeapon& Weapon);
	/**
	 * @brief The hook location with the truck hull's rotation; a tow socket on a turning part (a turret) must not
	 * swing the gun around with it.
	 */
	FTransform GetHitchFrame(const ATeamWeapon& Weapon) const;
	/**
	 * @brief Rest drawbar from the hook to the gun origin, lengthened so the trail end sits on the hook.
	 * Lengthened in the hitch frame, so the drawbar keeps its length when the truck pitches.
	 * @param Settings Tuning of the weapon.
	 * @param HitchFrame Hook location with the truck hull's rotation.
	 * @return World vector from the hook to where the gun origin rests before any sway.
	 */
	FVector GetRestDrawbar(const FTeamWeaponTowSwaySettings& Settings, const FTransform& HitchFrame) const;
	void StepSimulation(const FTeamWeaponTowSwaySettings& Settings, const ATeamWeapon& Weapon,
	                    const FTransform& HitchFrame, const float DeltaSeconds);
	float GetTrailerYawTarget(const FTeamWeaponTowSwaySettings& Settings, const FVector& HitchLocation,
	                          const FVector& RestDrawbarPlanarDirection, const float HitchTravelTowardsGunCm);
	// Updates hook velocity and the low-passed acceleration; returns how far the hook moved towards the gun.
	float UpdateHitchMotion(const FVector& HitchLocation, const FVector& RestPlanarDirection, const float DeltaSeconds);

	/**
	 * @brief Pitch and roll that put the swung gun's wheels on the ground, relative to its rigid pose on the truck.
	 * @param Settings Tuning of the weapon.
	 * @param Weapon The towed weapon, for its ground trace.
	 * @param HitchLocation World location of the hook.
	 * @param RestWorldTransform Weapon transform as attached by the truck.
	 * @param RestDrawbar World vector from the hook to the rest gun origin.
	 * @return Clamped targets plus the swung drawbar frame.
	 */
	FTeamWeaponTowSwayGroundTargets GetGroundTargets(const FTeamWeaponTowSwaySettings& Settings, const ATeamWeapon& Weapon,
	                                                 const FVector& HitchLocation, const FTransform& RestWorldTransform,
	                                                 const FVector& RestDrawbar) const;
	void StepSprings(const FTeamWeaponTowSwaySettings& Settings, const FTeamWeaponTowSwayGroundTargets& Targets,
	                 const float YawTarget, const float DeltaSeconds);
	FQuat GetSwayRotation(const FVector& RestDrawbar) const;

	bool bM_IsInitialised = false;
	// Weapon transform relative to the hitch frame as the truck attached it.
	FTransform M_RestRelativeTransform = FTransform::Identity;
	// Tow socket rotation in the truck's actor space when the gun was hooked.
	FQuat M_SocketRotationInTruckSpace = FQuat::Identity;
	// Rear reach of the gun mesh from its origin; the end of the trail.
	float M_TrailLengthCm = 0.f;
	// Pitch that put the wheels on the ground when the gun was hooked: the hook sits above the ground, so a
	// gun on level ground already hangs nose down. The pitch limits apply around this angle, not around zero.
	float M_SettledPitchDegrees = 0.f;
	// World point between the wheels that is dragged after the hook like a trailer axle.
	FVector M_AxleLocation = FVector::ZeroVector;
	FVector M_LastHitchLocation = FVector::ZeroVector;
	FVector M_HitchVelocity = FVector::ZeroVector;
	// Low-passed hook acceleration; raw frame to frame acceleration of a physics truck is noise.
	FVector M_HitchAcceleration = FVector::ZeroVector;
	float M_WheelHalfTrackCm = 0.f;
	FTeamWeaponTowSwaySpring M_Yaw;
	FTeamWeaponTowSwaySpring M_Pitch;
	FTeamWeaponTowSwaySpring M_Roll;
};
