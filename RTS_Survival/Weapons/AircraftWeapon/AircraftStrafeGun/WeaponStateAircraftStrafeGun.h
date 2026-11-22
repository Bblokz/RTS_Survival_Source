// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h"
#include "AircraftStrafeImpactBatcher.h"
#include "WeaponStateAircraftStrafeGun.generated.h"

class ALandscapeProxy;
class ASquadUnit;
class UAircraftGunAudioPlayer;

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

	// Impact radii below are set for this calibre; larger calibres scale with (calibre / reference)^exponent, as the
	// strafe impact VFX and the bullet pocks do.
	constexpr float ImpactReferenceCalibreMm = 7.92f;
	constexpr float ImpactCalibreScaleExponent = 0.6f;
	// Near misses (machine guns, no explosive filler): every impact drawn on the ground, a round or one of its extra
	// spurts, hits the nearest enemy soldier within this radius for one round's damage. The band of spurts stands for
	// the real rate of fire, so what the player sees landing next to a soldier hurts him.
	constexpr float NearMissRadiusAtReferenceCalibreCm = 90.f;
	// A soldier this far above or below an impact (on a roof, on a bridge) is not hit by it.
	constexpr float NearMissMaxHeightDifferenceCm = 300.f;
	// Strafe cannons with explosive filler splash on every round that lands instead (the spurts stay visual): radius
	// at the reference calibre, and the damage at the centre as a share of the round's damage.
	constexpr float CannonSplashRadiusAtReferenceCalibreCm = 90.f;
	constexpr float CannonSplashDamageShareOfRound = 0.5f;
	// The splash hurts vehicles through their rear armour: full damage up to the shrapnel pen, none from this multiple.
	constexpr float CannonSplashMaxArmorPenMlt = 1.5f;
	constexpr float CannonSplashArmorPenFallOff = 1.f;

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
	// Without aircraft gun audio data: one impact sound per gun at most this often (a strafe is dozens of impacts per
	// second). With it, UAircraftGunAudioPlayer plays a fire loop and an impact churn loop instead.
	constexpr float ImpactSoundIntervalSeconds = 0.12f;
}

/**
 * @brief Strafe gun of an aircraft: the weapon every multi-socket aircraft MG uses (made by
 * UAircraftWeapon::SetupMultiTraceWeapon, so no Blueprint changes). Each volley leaves along the nose into a
 * ground-plane beaten zone that walks with the aircraft: one ground probe and one unit search per volley instead of a
 * trace per round, extra visual spurts between the rounds, all drawn by UAircraftStrafeImpactBatcher when the tracers
 * arrive. Machine gun impacts near enemy soldiers hurt them (near misses); cannons with explosive filler splash instead.
 * Air-to-air fire and the -AircraftStrafeLegacyGuns switch keep the regular multi-trace behaviour.
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponStateAircraftStrafeGun : public UWeaponStateMultiTrace
{
	GENERATED_BODY()

public:
	// True when the command line asks for the old aircraft MG behaviour (A/B comparisons in the strafe test).
	static bool GetIsLegacyAircraftGunsRequested();

	// Aircraft cannons set up as projectile weapons in their Blueprint that UAircraftWeapon fires as strafe guns.
	static bool GetIsStrafeCannon(const EWeaponName WeaponName) { return WeaponName == EWeaponName::MK108_30MM_Horten; }
	// Tracer visual of the strafe cannons (a Blueprint projectile system does not apply to pooled tracers).
	static constexpr int32 StrafeCannonTracerType = 1;

	virtual void FireWeaponSystem() override;

	// Makes the fire loop + impact churn player (UAircraftGunAudioPlayer) now instead of on the first volley; called by
	// UAircraftWeapon when the weapon is set up. Does nothing without aircraft gun audio data.
	void InitGunAudio() { (void)GetOrCreateGunAudio(); }
	UAircraftGunAudioPlayer* GetGunAudio() const { return M_GunAudio; }

protected:
	// The strafe gun fires its tracers when a volley is resolved; only queued rounds skip the base tracer. Air-to-air
	// and fallback fire go through the regular multi-trace path and keep theirs.
	virtual bool GetFiresTracerOnTraceComplete() const override { return not bM_IsApplyingQueuedRound; }

	// Every volley: muzzle flashes as before; the per-volley launch sound is replaced by the gun audio's fire loop.
	virtual void CreateLaunchVfx(const FVector& LaunchLocation, const FVector& ForwardVector,
	                             bool bCreateShellCase = true) override;

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

	// Enemy soldiers under a volley, shared by its rounds for the near-miss check.
	using FNearMissSoldiers = TArray<TWeakObjectPtr<ASquadUnit>>;

	// A resolved round whose tracer is still in the air; impact (and damage, for a unit hit) waits for its arrival.
	struct FStrafeRoundInFlight
	{
		// Only for a round that hit a unit: the hit that the regular trace damage path resolves.
		TArray<FHitResult> UnitHits;
		// Null for cannons with explosive filler and for volleys with no enemy soldier near them.
		TSharedPtr<const FNearMissSoldiers> NearMissSoldiers;
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
	// Fire loop + impact churn of this gun; null when the RTS Audio Settings have no aircraft gun data.
	UPROPERTY()
	TObjectPtr<UAircraftGunAudioPlayer> M_GunAudio;
	bool bM_HasTriedCreatingGunAudio = false;
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

	// Weapons with explosive filler (the MK 108) splash on every round; the others hurt soldiers with near misses.
	bool GetUsesCannonSplash() const { return WeaponData.TNTExplosiveGrams > 0.f; }
	float GetRadiusForWeaponCalibre(float RadiusAtReferenceCalibreCm) const;
	TSharedPtr<const FNearMissSoldiers> CollectNearMissSoldiers(const TArray<FStrafeUnitCandidate>& Candidates) const;
	// A ground round and, when they are drawn, its extra spurts each hit the nearest soldier in reach.
	void ApplyNearMissDamage(const FStrafeRoundInFlight& Round);

	/**
	 * @brief One drawn impact hurts at most one soldier, through the regular damage and kill path so kill credit and
	 * veterancy work as for a direct hit.
	 * @param ImpactLocation Where the round or spurt lands.
	 * @param Soldiers The enemy soldiers under the volley.
	 * @param RadiusCm Reach of the impact on the ground.
	 */
	void ApplyNearMissAtImpact(const FVector& ImpactLocation, const FNearMissSoldiers& Soldiers, float RadiusCm);

	/**
	 * @brief Finds the soldier a near miss hurts: the closest one still alive and in view, so one spurt never hurts a
	 * whole squad standing close together.
	 * @param ImpactLocation Where the round or spurt lands.
	 * @param Soldiers The enemy soldiers under the volley.
	 * @param RadiusCm Reach of the impact on the ground.
	 * @return Null when no soldier is in reach.
	 */
	static ASquadUnit* FindNearestSoldierInReach(const FVector& ImpactLocation, const FNearMissSoldiers& Soldiers,
	                                             float RadiusCm);

	// Explosive filler: area damage around the round's impact, leaving out the unit it hit directly.
	void ApplyCannonSplash(const FStrafeRoundInFlight& Round) const;
	void PlayRoundImpactSound(const FStrafeRoundInFlight& Round);
	void PlayThrottledImpactSound(const FVector& Location);
	UAircraftGunAudioPlayer* GetOrCreateGunAudio();
	UAircraftStrafeImpactBatcher* GetReadyStrafeImpactBatcher() const;
	void StartArrivalTimerIfNeeded();
};
