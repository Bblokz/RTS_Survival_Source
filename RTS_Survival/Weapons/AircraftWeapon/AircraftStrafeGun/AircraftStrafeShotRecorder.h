// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

/**
 * @brief Map tests only: measures every aircraft MG round (regular multi-trace and strafe gun alike) so the strafe
 * test can compare both: angle to the nose, ground hits, distance to the aim point, and impact-vs-tracer timing.
 * Enabled by the strafe test; the weapons call it unconditionally and it ignores calls while disabled.
 */
class RTS_SURVIVAL_API FAircraftStrafeShotRecorder
{
public:
	static void Enable(const FString& Label);
	static void Disable();
	static bool GetIsEnabled();

	/**
	 * @brief One round leaves a gun.
	 * @param Weapon Firing weapon state, only used to tell guns apart in the CSV.
	 * @param MuzzleLocation Where the round leaves.
	 * @param FireDirection Direction of the round (not normalised).
	 * @param NoseForward Forward vector of the aircraft actor.
	 * @param AimTarget What the aircraft weapon is aiming at.
	 */
	static void RecordShot(const UObject* Weapon, const FVector& MuzzleLocation, const FVector& FireDirection,
	                       const FVector& NoseForward, const FVector& AimTarget);

	/**
	 * @brief A round's impact is drawn (and its damage applied).
	 * @param bHitSomething False when the trace hit nothing and the impact is drawn in the air.
	 * @param AimTarget Aim point to measure the miss distance against.
	 * @param FlightForward Aircraft forward when the round was fired; splits the miss along and across the flight.
	 * @param LaunchTime World time the round left the gun.
	 * @param ImpactTime World time the impact is drawn.
	 */
	static void RecordImpact(const UObject* Weapon, const bool bHitSomething, const FVector& LaunchLocation,
	                         const FVector& ImpactLocation, const FVector& AimTarget, const FVector& FlightForward,
	                         const float LaunchTime, const float ImpactTime);

	// A strafe gun volley probed the ground under its aim point; a miss puts the volley on the aim plane instead.
	static void RecordGroundProbe(const bool bFoundGround);

	// A machine gun impact (a round or a spurt) hurt the nearest enemy soldier; bKilled when it killed him.
	static void RecordNearMissHit(const bool bKilled);
	// A strafe cannon round with explosive filler splashed.
	static void RecordCannonSplash();
	static int32 GetNearMissHitCount();
	static int32 GetNearMissKillCount();
	static int32 GetCannonSplashCount();

	/**
	 * @brief An aircraft starts a dive: its later impacts are measured against what this dive attacks, so a test can
	 * tell a good first strafe run from later runs that miss.
	 * @param Aircraft The diving aircraft; its weapons' impacts count for this dive until its next one.
	 * @param Description Free text for the log (how the dive started).
	 * @param TargetActor The attacked unit, followed as it moves; null for a ground strafe.
	 * @param RunStart The marked ground point (or the unit's location when the dive started).
	 * @param RunEnd End of the strafe line on the ground; RunStart for an attack on a unit.
	 */
	static void BeginDive(const AActor* Aircraft, const FString& Description, const AActor* TargetActor,
	                      const FVector& RunStart, const FVector& RunEnd);

	// Logs one RTS_STRAFE_DIVE line per dive since Enable and a summary of the dives after each aircraft's first.
	static void LogDives();

	// Dives after each aircraft's first one that put at least one round near what they attacked / all of them.
	static int32 GetLaterDivesOnTargetCount();
	static int32 GetLaterDivesWithImpactsCount();

	// One line with every metric, for the RTS_AIRCRAFT_TEST log.
	static FString DescribeMetrics();

	static int32 GetShotCount();
	static int32 GetImpactCount();
	static float GetGroundHitShare();
	static float GetShareOfShotsOutsideNoseCone();
	static float GetMeanAbsImpactLeadSeconds();
	static float GetMeanCrossTrackMissCm();

	// Writes every recorded round to Saved/AircraftStrafeQA/<Label>.csv for the top-down plots.
	static void WriteCsv();
};

#endif
