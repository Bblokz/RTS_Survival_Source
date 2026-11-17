// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h"
#include "WeaponStateAircraftStrafeGun.generated.h"

namespace AircraftStrafeGunSettings
{
	// Fixed forward guns: rounds leave within this cone around the nose, so fire always visibly follows the aircraft.
	// Depression is generous because the strafe dives are shallow; a real gun sight is set up the same way.
	constexpr float MaxYawFromNoseDeg = 10.f;
	constexpr float MaxDepressionBelowNoseDeg = 35.f;
	constexpr float MaxElevationAboveNoseDeg = 4.f;

	// Beaten zone: cross-track spread (1 sigma, cm) at the reference distance for a weapon with 0 accuracy.
	constexpr float CrossTrackSigmaAtReferenceCm = 340.f;
	constexpr float ReferenceDistanceCm = 3000.f;
	// Rounds striking the ground at a shallow angle smear along the flight line; capped so a level pass stays readable.
	constexpr float MaxAlongTrackStretch = 3.f;
	// Samples beyond this many sigma are pulled back so no stray round lands far outside the zone.
	constexpr float MaxSigmaClamp = 2.2f;
	// Wing guns keep this share of their spacing on the ground: parallel lines of spurts instead of one point.
	constexpr float WingGunSpacingKeptOnGround = 1.f;

	// Traces run this far past the sampled ground point so terrain slightly below the aim plane is still found.
	constexpr float TraceOvershootCm = 600.f;
	// How often the queue of rounds still in the air is checked for arrivals.
	constexpr float ArrivalCheckIntervalSeconds = 1.f / 60.f;
	// Rounds in the air per gun before new arrivals are applied instantly instead of queued.
	constexpr int32 MaxRoundsInFlight = 256;
}

/**
 * @brief Strafe gun of an aircraft: the trace weapon every multi-socket aircraft MG uses (made by
 * UAircraftWeapon::SetupMultiTraceWeapon, so no Blueprint changes). Rounds leave along the nose, land in a
 * ground-plane beaten zone that walks with the aircraft, and their impacts and damage wait for the tracer to arrive.
 * Air-to-air fire and the -AircraftStrafeLegacyGuns switch keep the regular multi-trace behaviour.
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponStateAircraftStrafeGun : public UWeaponStateMultiTrace
{
	GENERATED_BODY()

public:
	// True when the command line asks for the old aircraft MG behaviour (A/B comparisons in the strafe test).
	static bool GetIsLegacyAircraftGunsRequested();

	virtual void FireWeaponSystem() override;

protected:
	virtual bool GetFiresTracerOnTraceComplete() const override { return false; }

private:
	struct FStrafeGunAim
	{
		// Direction of the gun line after the nose cone clamp.
		FVector GunLineDirection = FVector::ForwardVector;
		// Where the gun line meets the aim plane, or the end of its reach when it never does.
		FVector AimCentre = FVector::ZeroVector;
		FVector FlightForward2D = FVector::ForwardVector;
		FVector FlightRight2D = FVector::RightVector;
		FVector GunsCentre = FVector::ZeroVector;
		float SlantDistance = 0.f;
		float AlongTrackStretch = 1.f;
		bool bReachesGround = false;
	};

	// A round whose trace is resolved but whose tracer is still in the air.
	struct FStrafeRoundInFlight
	{
		TArray<FHitResult> Hits;
		FVector LaunchLocation = FVector::ZeroVector;
		FVector TraceEnd = FVector::ZeroVector;
		// Aim point and flight direction when fired; only the strafe QA recorder reads them.
		FVector AimCentre = FVector::ZeroVector;
		FVector FlightForward2D = FVector::ForwardVector;
		float LaunchTime = 0.f;
		float ArrivalTime = 0.f;
	};

	TArray<FStrafeRoundInFlight> M_RoundsInFlight;
	FTimerHandle M_ArrivalTimerHandle;

	bool GetShouldUseRegularMultiTrace() const;
	bool ComputeStrafeGunAim(FStrafeGunAim& OutAim) const;
	FVector ClampDirectionToNoseCone(const FVector& DesiredDirection, const FRotator& NoseRotation) const;
	FVector GetGunsCentre() const;

	/**
	 * @brief Picks where one gun's round lands: wing gun spacing kept, then a random offset in the ground plane that
	 * is longer along the flight line, scaled with distance and the weapon's inaccuracy.
	 * @param Aim The gun line of this round.
	 * @param MuzzleLocation The gun's muzzle.
	 * @return Ground point the trace aims at.
	 */
	FVector SampleBeatenZonePoint(const FStrafeGunAim& Aim, const FVector& MuzzleLocation) const;

	void FireRoundFromSocket(const FName& SocketName, const FStrafeGunAim& Aim);
	// Fires the tracer now and queues the impact for when the tracer arrives; Round holds the shot's launch data.
	void OnStrafeRoundTraceComplete(const FTraceDatum& TraceDatum, const FStrafeRoundInFlight& Round);
	void QueueRoundArrival(FStrafeRoundInFlight&& Round);
	void ApplyArrivedRounds();
	void ApplyRoundImpact(const FStrafeRoundInFlight& Round);
	void StartArrivalTimerIfNeeded();
};
