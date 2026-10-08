#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderTypes.h"

/** Designer values that decide how good a cover point is in a firefight; copied from the cover settings. */
struct FRTSCombatCoverSettings
{
	// Widest yaw, left or right of looking straight at the cover, at which the cover aim offsets can still aim.
	float MaximumAimYawDegrees = 90.0f;

	// An enemy within this yaw of straight ahead has the cover between itself and the soldier.
	float ProtectedHalfAngleDegrees = 60.0f;

	float ProtectionWeight = 1.0f;
	float FacingWeight = 0.5f;
	float TravelWeight = 0.4f;

	// Walking this far to a point costs the full travel weight.
	float TravelReferenceDistance = 900.0f;

	// A soldier only gives up its point for one that scores at least this much higher.
	float MinimumScoreGain = 0.3f;
};

/** The enemies a soldier wants cover from. The first threat is the target the soldier is shooting at. */
struct FRTSCombatCoverThreats
{
	FVector PrimaryTargetLocation = FVector::ZeroVector;

	// Every enemy the soldier's squad is engaging, the primary target included.
	TArray<FVector, TInlineAllocator<8>> ThreatLocations;
};

/**
 * @brief Rates a cover point against where the enemy actually is, so soldiers in a firefight pick and switch to
 * cover that stands between them and the incoming fire. Pure value logic shared by the game and its tests.
 */
struct FRTSCombatCoverScoring
{
	/** @return Signed yaw from looking straight at the cover to Location; positive is to the soldier's right. */
	static float GetYawToLocationDegrees(const FRTSCoverPoint& CoverPoint, const FVector& Location);

	/** @return True when a soldier in this cover can turn its aim offset far enough to aim at the target. */
	static bool GetCanAimAt(
		const FRTSCoverPoint& CoverPoint,
		const FVector& TargetLocation,
		const FRTSCombatCoverSettings& Settings);

	/** @return Share of the threats, 0 to 1, that have the cover between themselves and the soldier. */
	static float GetProtectedThreatFraction(
		const FRTSCoverPoint& CoverPoint,
		const FRTSCombatCoverThreats& Threats,
		const FRTSCombatCoverSettings& Settings);

	/**
	 * @brief Scores a point for a soldier standing at UnitLocation; higher is better.
	 * @param CoverPoint Candidate point.
	 * @param UnitLocation Where the soldier is now; walking further costs score.
	 * @param Threats The enemies to take cover from.
	 * @param Settings Designer weights and angles.
	 * @param OutScore The score, only written when the point is usable.
	 * @return False when the primary target cannot be aimed at from this point.
	 */
	static bool TryScoreCoverPoint(
		const FRTSCoverPoint& CoverPoint,
		const FVector& UnitLocation,
		const FRTSCombatCoverThreats& Threats,
		const FRTSCombatCoverSettings& Settings,
		float& OutScore);

	/** @return True when a candidate's score is enough better than the occupied point's to be worth the walk. */
	static bool GetIsWorthRepositioning(
		float CurrentPointScore,
		float CandidateScore,
		const FRTSCombatCoverSettings& Settings);
};
