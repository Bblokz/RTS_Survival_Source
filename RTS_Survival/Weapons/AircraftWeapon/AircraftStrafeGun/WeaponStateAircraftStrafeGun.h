// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h"
#include "AircraftStrafeImpactBatcher.h"
#include "WeaponStateAircraftStrafeGun.generated.h"

class ALandscapeProxy;

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
	// Nose guns (Bf 110) sit close together; this keeps their band a strafe band instead of one line.
	constexpr float MinCrossTrackSigmaCm = 160.f;
	// Rounds striking the ground at a shallow angle smear along the flight line; capped so a level pass stays readable.
	constexpr float MaxAlongTrackStretch = 3.f;
	// Samples beyond this many sigma are pulled back so no stray round lands far outside the zone.
	constexpr float MaxSigmaClamp = 2.2f;
	// Wing guns keep this share of their spacing on the ground: parallel lines of spurts instead of one point.
	constexpr float WingGunSpacingKeptOnGround = 1.f;

	// Purely visual spurts around every round's impact (no damage, no trace): the dense churned band of a strafe.
	constexpr int32 ExtraSpurtsPerRound = 3;
	// Share of the rounds that land on the ground leaving a bullet pock decal (the extra spurts leave none).
	constexpr float BulletPockChancePerRound = 0.6f;
	// Spread of those spurts around their round, as a share of the zone's spread.
	constexpr float ExtraSpurtSpreadShareOfZone = 0.45f;

	// One probe per volley finds the ground under the aim point; it starts this far above and ends this far below.
	constexpr float GroundProbeAboveCm = 3000.f;
	constexpr float GroundProbeBelowCm = 4000.f;
	// Steeper ground than this is treated as this steep when placing impacts on the probed plane.
	constexpr float MinGroundNormalZ = 0.3f;
	// The unit search reaches this far past the outermost round, for units whose centre is outside the zone.
	constexpr float UnitSearchMarginCm = 800.f;
	// A round confirmed on a unit traces this far past its ground point to find the unit's surface and armour.
	constexpr float UnitHitTraceOvershootCm = 300.f;

	// Traces run this far past the aim point when the gun line never reaches the ground.
	constexpr float TraceOvershootCm = 600.f;
	// How often the queue of rounds still in the air is checked for arrivals.
	constexpr float ArrivalCheckIntervalSeconds = 1.f / 60.f;
	// Rounds in the air per gun before new arrivals are applied instantly instead of queued.
	constexpr int32 MaxRoundsInFlight = 256;
	// One impact sound per gun at most this often; a strafe is dozens of impacts per second.
	constexpr float ImpactSoundIntervalSeconds = 0.12f;
}

/**
 * @brief Strafe gun of an aircraft: the weapon every multi-socket aircraft MG uses (made by
 * UAircraftWeapon::SetupMultiTraceWeapon, so no Blueprint changes). Each volley leaves along the nose into a
 * ground-plane beaten zone that walks with the aircraft: one ground probe and one unit search per volley instead of a
 * trace per round, extra visual spurts between the rounds, all drawn by UAircraftStrafeImpactBatcher when the tracers
 * arrive. Air-to-air fire and the -AircraftStrafeLegacyGuns switch keep the regular multi-trace behaviour.
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
	// The strafe gun fires its tracers when a volley is resolved; only queued rounds skip the base tracer. Air-to-air
	// and fallback fire go through the regular multi-trace path and keep theirs.
	virtual bool GetFiresTracerOnTraceComplete() const override { return not bM_IsApplyingQueuedRound; }

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
		float CrossTrackSigma = 0.f;
		bool bReachesGround = false;
	};

	// One gun's round of a volley before the ground and units are known: its muzzle and where it lands (x, y).
	struct FStrafeRoundPlan
	{
		FVector LaunchLocation = FVector::ZeroVector;
		FVector AimPoint = FVector::ZeroVector;
	};

	struct FStrafeVolley
	{
		FStrafeGunAim Aim;
		TArray<FStrafeRoundPlan> Rounds;
		float LaunchTime = 0.f;
	};

	// The ground under a volley, from its probe: impacts are placed on this plane.
	struct FStrafeGroundPlane
	{
		FVector Point = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		EAircraftStrafeImpactSurface Surface = EAircraftStrafeImpactSurface::Dirt;
		TWeakObjectPtr<UPhysicalMaterial> PhysicalMaterial;
		// Set when the probe hit a landscape: impacts are snapped to its drawn surface, as the probe hits its simple
		// collision, which can sit most of a metre off it (flat pocks would vanish under the ground).
		TWeakObjectPtr<ALandscapeProxy> Landscape;
	};

	// A unit that may stand under the volley, with its footprint in its own space.
	struct FStrafeUnitCandidate
	{
		TWeakObjectPtr<AActor> Actor;
		FTransform ActorTransform;
		FBox LocalBounds;
	};

	// A resolved round whose tracer is still in the air; impact (and damage, for a unit hit) waits for its arrival.
	struct FStrafeRoundInFlight
	{
		// Only for a round that hit a unit: the hit that the regular trace damage path resolves.
		TArray<FHitResult> UnitHits;
		FVector LaunchLocation = FVector::ZeroVector;
		FVector ImpactLocation = FVector::ZeroVector;
		FVector ImpactNormal = FVector::UpVector;
		TArray<FVector> ExtraSpurts;
		TWeakObjectPtr<UPhysicalMaterial> GroundMaterial;
		EAircraftStrafeImpactSurface Surface = EAircraftStrafeImpactSurface::Dirt;
		bool bHasImpact = false;
		// Aim point and flight direction when fired; only the strafe QA recorder reads them.
		FVector AimCentre = FVector::ZeroVector;
		FVector FlightForward2D = FVector::ForwardVector;
		float LaunchTime = 0.f;
		float ArrivalTime = 0.f;
	};

	TArray<FStrafeRoundInFlight> M_RoundsInFlight;
	FTimerHandle M_ArrivalTimerHandle;
	float M_LastImpactSoundTime = -1.f;
	// True only while a queued round runs the base trace path (damage, regular impact): its tracer already flew.
	bool bM_IsApplyingQueuedRound = false;

	bool GetShouldUseRegularMultiTrace() const;
	bool ComputeStrafeGunAim(FStrafeGunAim& OutAim) const;
	FVector ClampDirectionToNoseCone(const FVector& DesiredDirection, const FRotator& NoseRotation) const;
	FVector GetGunsCentre() const;

	/**
	 * @brief Picks where one gun's round lands: wing gun spacing kept, then a random offset in the ground plane that
	 * is longer along the flight line, scaled with distance and the weapon's inaccuracy.
	 * @param Aim The gun line of this volley.
	 * @param MuzzleLocation The gun's muzzle.
	 * @return Point on the aim plane the round heads for.
	 */
	FVector SampleBeatenZonePoint(const FStrafeGunAim& Aim, const FVector& MuzzleLocation) const;

	void PlanVolley(const FStrafeGunAim& Aim, FStrafeVolley& OutVolley);
	void RequestGroundProbe(const FStrafeVolley& Volley);
	void OnGroundProbeComplete(const FTraceDatum& TraceDatum, const FStrafeVolley& Volley);

	/**
	 * @brief Resolves every round of a volley: where it strikes the ground, which unit (if any) it hits, its extra
	 * spurts; fires the tracers now and queues the impacts for when the tracers arrive.
	 * @param Volley The planned rounds.
	 * @param Ground The ground plane under the volley; null when the gun line does not reach the ground.
	 */
	void ResolveVolley(const FStrafeVolley& Volley, const FStrafeGroundPlane* Ground);
	TArray<FStrafeUnitCandidate> FindUnitsUnderVolley(const FStrafeVolley& Volley, const FStrafeGroundPlane& Ground) const;
	const FStrafeUnitCandidate* FindUnitUnderPoint(const TArray<FStrafeUnitCandidate>& Candidates,
	                                               const FVector& GroundPoint) const;
	bool TraceRoundIntoUnit(const FStrafeRoundPlan& Round, const FVector& GroundPoint, const AActor& Unit,
	                        FHitResult& OutHit) const;
	FVector ProjectOntoGround(const FVector& Point, const FStrafeGroundPlane& Ground) const;
	void AddExtraSpurts(const FStrafeVolley& Volley, const FStrafeGroundPlane& Ground, FStrafeRoundInFlight& Round) const;

	void QueueRoundArrival(FStrafeRoundInFlight&& Round);
	void ApplyArrivedRounds();
	void ApplyRoundImpact(const FStrafeRoundInFlight& Round);
	// Runs the regular trace path (armour, damage, impact) for a queued round without a second tracer.
	void ApplyQueuedRoundThroughTracePath(FTraceDatum& TraceDatum, const FStrafeRoundInFlight& Round);
	void DrawGroundImpact(const FStrafeRoundInFlight& Round);
	void PlayThrottledImpactSound(const FVector& Location);
	UAircraftStrafeImpactBatcher* GetReadyStrafeImpactBatcher() const;
	void StartArrivalTimerIfNeeded();
};
