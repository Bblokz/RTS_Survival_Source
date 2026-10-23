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

	// Walking this far to a point costs the full travel weight; longer walks cost proportionally more.
	float TravelReferenceDistance = 900.0f;

	// A soldier only gives up its point for one that scores at least this much higher.
	float MinimumScoreGain = 0.3f;

	// Share of the weapon range that combat cover keeps between the soldier and its target. A share of the
	// range lets short-ranged weapons such as SMGs and flamethrowers close in further than rifles. Zero: no limit.
	float MinimumEngagementRangeRatio = 0.35f;

	// How much closer to its target a soldier may get by walking to combat cover. Zero: no limit.
	float MaximumAdvanceTowardTarget = 600.0f;
};

/** How far from its target a soldier's combat cover may be. A zero field disables that limit. */
struct FRTSCombatCoverDistanceLimits
{
	// Keeps the point inside weapon range of the target.
	float MaximumDistanceToTarget = 0.0f;

	// Keeps the soldier at a firing distance instead of walking up to point-blank range for a better wall.
	float MinimumDistanceToTarget = 0.0f;

	float MaximumAdvanceTowardTarget = 0.0f;
};

/** What a soldier in cover does when that cover turns out to be useless against its current target. */
enum class ERTSCoverInvalidTargetDecision : uint8
{
	// Stay hidden for now: the weapon may switch back, or the target may move back into reach.
	StayInCover,
	LeaveCover
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

	/**
	 * @brief Decides whether a soldier gives up cover it cannot engage its current target from.
	 * A weapon changes target often in a firefight; leaving for each change puts the soldier in the open.
	 * @param bTargetIsAttackOrder True when the target is the enemy the squad was ordered to attack; an order
	 * is never waited out.
	 * @param SecondsWithoutValidTarget How long the point has been useless against whatever the target was.
	 * @param ToleranceSeconds Designer setting; zero restores leaving at once.
	 * @return Whether to stay hidden or leave.
	 */
	static ERTSCoverInvalidTargetDecision DecideOnInvalidTarget(
		bool bTargetIsAttackOrder,
		float SecondsWithoutValidTarget,
		float ToleranceSeconds);

	/** @return True when a candidate's score is enough better than the occupied point's to be worth the walk. */
	static bool GetIsWorthRepositioning(
		float CurrentPointScore,
		float CandidateScore,
		const FRTSCombatCoverSettings& Settings);

	// Cover must stay clearly inside weapon range, or entering it would make the squad walk closer again.
	static float GetMaximumCoverDistanceToTarget(float WeaponRange);

	/**
	 * @brief Derives the distance limits from the weapon, so each soldier keeps the distance its weapon fights at.
	 * @param WeaponRange Maximum range of the soldier's weapon; zero disables every limit.
	 * @param Settings Designer ratio and advance limit.
	 * @return The limits for that weapon.
	 */
	static FRTSCombatCoverDistanceLimits BuildDistanceLimits(float WeaponRange, const FRTSCombatCoverSettings& Settings);

	/**
	 * @brief Rejects points that would take the soldier to point-blank range or far forward toward its target,
	 * however well they shield it. A target that walked up itself never forbids cover no closer than the soldier.
	 * @param CoverPoint Candidate point.
	 * @param UnitLocation Where the soldier is now.
	 * @param TargetLocation Where the soldier's target is.
	 * @param Limits Distance limits of the soldier's weapon.
	 * @return True when the point respects every enabled limit.
	 */
	static bool GetIsWithinDistanceLimits(
		const FRTSCoverPoint& CoverPoint,
		const FVector& UnitLocation,
		const FVector& TargetLocation,
		const FRTSCombatCoverDistanceLimits& Limits);
};
