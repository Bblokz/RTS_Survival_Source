// FAircraftPath.h

#pragma once

#include "CoreMinimal.h"
#include "AircraftPathResults/AircraftPathResults.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Aircraft/AircaftHelpers/FRTSAircraftHelpers.h"
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"
#include "FAircraftPath.generated.h"


UENUM()
enum class EAirPathPointType :uint8
{
	Regular,
	Bezier,
	AttackDive,
	GetOutOfDive,
};

inline FString Global_GetAircraftPathPointTypeAsString(const EAirPathPointType PointType)
{
	switch (PointType)
	{
	case EAirPathPointType::Regular:
		return TEXT("Regular");
	case EAirPathPointType::Bezier:
		return TEXT("Bezier");
	case EAirPathPointType::AttackDive:
		return TEXT("Attack Dive");
	case EAirPathPointType::GetOutOfDive:
		return TEXT("Get Out Of Dive");
	default:
		return TEXT("Unknown");
	}
}

struct FDistAbsAngleSign
{
	float Distance = 0.f;
	float AbsAngle = 0.f;
	// Sign of the angle, 1 for positive, -1 for negative.
	int32 Sign = 1;

	FVector LeftRightUnitVectorToEnemy = FVector::ZeroVector;

	FVector GetDirection() const
	{
		return FVector::ForwardVector.RotateAngleAxis(AbsAngle * Sign, FVector::UpVector);
	}

	bool IsOnTheRight() const
	{
		return Sign > 0;
	}

	FDistAbsAngleSign() = default;

	FDistAbsAngleSign(float InDistance, float InAbsAngle, int32 InSign)
		: Distance(InDistance), AbsAngle(InAbsAngle), Sign(InSign)
	{
	}

	void Debug(const float DebugTime) const
	{
		if constexpr (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
		{
			const FString DebugString = FString::Printf(
				TEXT("Distance: %.2f, AbsAngle: %.2f, Sign: %d"),
				Distance, AbsAngle, Sign);
			FRTSAircraftHelpers::AircraftDebug(DebugString, FColor::Blue);
		}
	}
};


USTRUCT()
struct FAircraftPathPoint
{
	GENERATED_BODY()

	bool IsOnBezierCurve() const
	{
		return PointType == EAirPathPointType::Bezier;
	}

	bool IsDivePoint() const
	{
		return PointType == EAirPathPointType::AttackDive;
	}

	bool IsDiveRecoveryPoint() const
	{
		return PointType == EAirPathPointType::GetOutOfDive;
	}

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	double Roll = 0.f;

	UPROPERTY()
	EAirPathPointType PointType = EAirPathPointType::Regular;

	// Dive and dive-recovery points: the flight toward them does not descend below this height (ground of the target
	// plus a margin); it does not move with terrain following. Unset (lowest float) on every other point.
	UPROPERTY()
	float FloorZ = TNumericLimits<float>::Lowest();

	bool GetHasFloor() const { return FloorZ > TNumericLimits<float>::Lowest(); }
};

USTRUCT()
struct FAircraftPath
{
	GENERATED_BODY()

	void Reset();

	void CreateMoveToPath(
		const FVector& StartAirPoint,
		const FVector& EndAirPoint,
		const FRotator& StartRotation,
		const FAircraftBezierCurveSettings& BezierCurveSettings, const FAircraftDeadZone& DeadZoneSettings);

	void CreateAttackPath(
		const FVector& StartAirPoint,
		const FRotator& StartRotation,
		const FVector& EnemyLocation,
		const FAircraftBezierCurveSettings& BezierCurveSettings, const FAircraftAttackMoveSettings& AttackMoveSettings,
		const
		FAircraftDeadZone& DeadZoneSettings
	);

	void DebugPath(const float DebugTime);

	/** @brief Moves every point up or down with the aircraft's cruise height, so the path keeps its shape. */
	void ShiftHeight(const float DeltaZ);

	/** @return Whether the aircraft is flying into a dive or out of it: the part of an attack run that aims. */
	bool GetIsFlyingDiveSegment() const;

	/**
	 * @brief Bank-to-turn path following: whether the aircraft has passed the point it flies toward. True past the end
	 * of the segment, within the acceptance radius, or when it already flies along the next segment after rounding the
	 * corner. (No "as far from the start as the segment is long" rule: off to the side of a long last leg it ended
	 * move orders kilometres early.)
	 * @param Location Aircraft location.
	 * @param AcceptanceRadius Distance to the point that counts as reached.
	 */
	bool GetShouldAdvancePastTargetPoint(const FVector& Location, const float AcceptanceRadius) const;

	/**
	 * @brief The point LookAheadDistance further along the path than the aircraft's projection on its segment; past
	 * the last point it continues along the last segment, so the heading stays calm on arrival.
	 * @param Location Aircraft location.
	 * @param LookAheadDistance Distance along the path (cm).
	 * @return Where the aircraft aims this frame.
	 */
	FVector GetLookAheadPoint(const FVector& Location, const float LookAheadDistance) const;

	/** @return The floor of the segment flown now (dive and recovery); unset on every other segment. */
	TOptional<float> GetFloorZOfCurrentSegment() const;

	/** @return Path length still ahead of the aircraft: from its projection on its segment to the last point (cm). */
	float GetRemainingLength(const FVector& Location) const;

	UPROPERTY()
	TArray<FAircraftPathPoint> PathPoints;

	UPROPERTY()
	int32 CurrentPathIndex = 0;

	UPROPERTY()
	UObject* Owner = nullptr;

	// Bank-to-turn flight: radius of the turns the aircraft flies at cruise speed. Above zero, move and attack paths
	// are planned as one turn at this radius and then a straight line (no S-shaped bezier the aircraft cannot fly),
	// and a dive is only planned when the aircraft can turn onto the target in time. 0 keeps the bezier planning.
	float PlanningTurnRadius = 0.f;

	// Bank-to-turn flight: side the aircraft already turns to (1 right, -1 left, 0 none); a target far off the nose is
	// turned to on this side, so a new order does not reverse the bank.
	float PreferredTurnSign = 0.f;

	// Bank-to-turn flight: how many attack plans past the allowed number ended without a dive. Each level widens the
	// dive cone a little and lowers the turn-onto-target margin, so an aircraft never circles a target forever
	// without jumping straight to a wide, banked dive.
	int32 DiveRelaxLevel = 0;

	// Bank-to-turn attack approach: the run-in ends this far from the target (0 when the path has no run-in); the
	// aircraft keeps that end on the line to the target while it flies the run-in, so a moving target is not lost.
	float RunInEndDistanceToTarget = 0.f;

	/**
	 * @brief Moves the last point (the run-in end) onto the line from the aircraft to the target's current position,
	 * at the planned distance from it, while the aircraft flies the last segment.
	 * @param AircraftLocation Aircraft location.
	 * @param TargetLocation Where the target is now.
	 */
	void ReaimRunInEnd(const FVector& AircraftLocation, const FVector& TargetLocation);

private:
	/**
	 * @brief Adds the points of one turn at PlanningTurnRadius from the start toward the end, up to where the straight
	 * line to the end leaves the circle; an end inside the circle first gets a straight fly-out just long enough to
	 * put it outside, so there is always one turn and never a hairpin.
	 * @pre The start point is already in the path. Adds neither the start nor the end point.
	 * @param StartAirPoint Where the turn begins.
	 * @param EndAirPoint Where the straight line after the turn goes.
	 * @param StartRotation Heading at the start (only its yaw is used).
	 */
	void AddTurnRadiusArc(const FVector& StartAirPoint, const FVector& EndAirPoint, const FRotator& StartRotation);

	/**
	 * @brief An end inside the turn circle cannot be reached by turning at once: adds the straight fly-out point that
	 * puts it just on the circle's edge.
	 * @param StartAirPoint Where the turn would begin.
	 * @param EndAirPoint Where the line after the turn goes.
	 * @param Forward Level heading at the start.
	 * @param TurnSign 1 turns right, -1 left.
	 * @return Where the turn begins: the fly-out point, or the start when no fly-out is needed.
	 */
	FVector AddFlyOutIfEndInsideTurn(const FVector& StartAirPoint, const FVector& EndAirPoint, const FVector& Forward,
	                                 float TurnSign);

	/**
	 * @brief Bank-to-turn attack approach: turn toward the enemy at PlanningTurnRadius, then a straight, steadying
	 * run-in to where the dive starts; a target too close for that is first overflown far enough to come back to.
	 * The dive itself is planned when this path ends, with the aircraft aligned (no dive starts mid-turn).
	 * @pre The start point is already in the path.
	 */
	void AddTurnRadiusAttackApproach(const FVector& StartAirPoint, const FRotator& StartRotation,
	                                 const FVector& EnemyAirLocation, const FAircraftAttackMoveSettings& AttackMoveSettings);

	/** @return Whether a turn at PlanningTurnRadius can bring the nose onto a target this far and this far off it. */
	bool GetCanTurnOntoTarget(float Distance, float AbsAngleDeg) const;

	/** @return Straight flight (cm) the aircraft needs after a turn to steady its nose before a dive. */
	float GetDiveAlignLength() const;

	/**
	 * @brief Adds a move-to path from the start point to the end point. Uses different bezier curves
	 * depending on the angle and the distance to the target.
	 * @pre The path was reset and now has all points already that come before this manoeuvre including the
	 * StartAirPoint!
	 * @param bIgnoreBezierMaxDistance: If true, the bezier curve will not be limited by the maximum distance
	 * this means the end point will not be changed.
	 * @post The bezier-curve and end point are added to the PathPoints.
	 * @note This function does not set any rotations for the points, only add the locations and their types.
	 */
	void AddMoveToPathFromStart(
		const FVector& StartAirPoint,
		const FVector& EndAirPoint,
		const FRotator& StartRotation,
		const FAircraftBezierCurveSettings& BezierCurveSettings,
		const FAircraftDeadZone& DeadZoneSettings,
		const bool bIgnoreBezierMaxDistance = false);

	/**
	 * @brief Adds a uturn approach point, u turn bezier points and the final u turn point to the pathing.
	 * @pre The StartLocation was already added to the path.
	 * @post No Rotations set for bezier but all pathing points are now added to complete the U-turn path.
	 */
	void AddUTurnPathingToEnemy(
		const FVector& StartLocation,
		const FRotator& StartRotation,
		const FVector& EnemyAirLocation, const FAircraftBezierCurveSettings& BezierCurveSettings, const
		FAircraftAttackMoveSettings& AttackMoveSettings, const FDistAbsAngleSign& DistAbsAngle);

	FVector GetUTurnPointsInRangeToActor(
		const FDistAbsAngleSign& DistanceAngle,
		const FAircraftAttackMoveSettings& AttackMoveSettings,
		const FVector& EnemyAirPosition, FVector& UTurnEndPoint, const FRotator& StartRotation,
		const FVector& StartLocation, const
		float BezierCurveTension
	) const;

	bool GetIsExtremeAngleToEnemy(
		const FDistAbsAngleSign& DistanceAngle) const;
	FVector GetOffsetLocationInAttackRange(
		const FVector& EnemyAirLocation,
		const FVector& OffsetLocation,
		const FAircraftAttackMoveSettings& AttackMoveSettings) const;

	/** @return Whether distance exceeds Max attack distance scaled by setting*/ 
	bool GetIsEnemyExtremelyFar(
		const FDistAbsAngleSign& DistanceAngle,
		const FAircraftAttackMoveSettings& AttackMoveSettings) const;


	void AddAttackPathTooFar(const FVector& StartAirPoint, const FRotator& StartRotation,
	                         const FVector& EnemyAirLocation,
	                         const FAircraftBezierCurveSettings& BezierCurveSettings,
	                         const FAircraftAttackMoveSettings& AttackMoveSettings,
	                         const FAircraftDeadZone& DeadZoneSettings, const FDistAbsAngleSign& DistanceAngle, FVector& OutEndAirPoint);

	/**
	 * @brief Adds bezier points and rotations to recover an extreme angle towards the enemy.
	 * @post The bezier-curve and rotations are added to the path to reduce the angle to the enemy once finished.
	 * @note THe idea is to use this recursively; if the enemy is still alive after this path finished we are now on a better
	 * angle for attacking.
	 */
	void AddAttackPathTooFar_ExtremeAngle
	(const FVector& StartAirPoint, const FRotator& StartRotation,
	 const FAircraftBezierCurveSettings& BezierCurveSettings,
	 const FAircraftAttackMoveSettings& AttackMoveSettings,
	 const FAircraftDeadZone& DeadZoneSettings, const
	 FDistAbsAngleSign& DistanceAngle, FVector& OutEndAirPoint);

	/**
	* @brief We perform a bezier curve movement as follows:
	* We put the end point at the line from enemy to aircraft with distance of 90% of max attack distance away
	* from the enemy and perform a regular move to with bezier to that point.
	 */
	void AddAttackPathTooFar_ExtremeDistance
	(const FVector& StartAirPoint, const FRotator& StartRotation,
	 const FVector& EnemyAirLocation,
	 const FAircraftBezierCurveSettings& BezierCurveSettings,
	 const FAircraftAttackMoveSettings& AttackMoveSettings, const FAircraftDeadZone& DeadZoneSettings, FVector& OutEndAirPoint);

	/**
	 * @brief We perform a bezier move to a location projected within the attack margins on the vector from the player
	 * to the enemy.
	 */
	void AttackPathTooFar_WithinAngle(
		const FVector& StartAirPoint,
		const FRotator& StartRotation,
		const FVector& EnemyAirLocation,
		const FDistAbsAngleSign& DistanceAngle,
		const FAircraftBezierCurveSettings& BezierCurveSettings, const FAircraftAttackMoveSettings& AttackMoveSettings,
		const FAircraftDeadZone& DeadZoneSettings, FVector& OutEndAirPoint);


	void AddAttackPathTooClose(const FVector& StartAirPoint, const FRotator& StartRotation,
	                           const FAircraftAttackMoveSettings& AttackMoveSettings);

	EDirectAttackDiveResult GetIsWithinDirectAttackDiveRange(
		const FVector& StartAirPoint,
		const FRotator& StartRotation,
		const FVector& EnemyAirLocation, const FAircraftAttackMoveSettings& AttackMoveSettings, FDistAbsAngleSign&
		DistanceAngleToTarget) const;

	void AddAttackDivePath(
		const FVector& DiveStartPoint,
		const FVector& EnemyLocation,
		const FAircraftAttackMoveSettings& AttackMoveSettings, FVector& OutFinalLocation);

	bool GetIsWithinDeadZone(const FRotator& StartRotation, const FVector& Start,
	                         const FVector& PointToCheck, const FAircraftDeadZone& DeadZoneSettings,
	                         float& OutAngle) const;

	bool GetIsEndPointOutsideBezierReach(const FVector& StartAirPoint, const FVector& EndAirPoint,
	                                     const FAircraftBezierCurveSettings& BezierCurveSettings) const;
	FVector GetBezierEndInReach(const FVector& StartAirPoint, const FVector& EndAirPoint,
	                            const FAircraftBezierCurveSettings& BezierCurveSettings) const;

	/**
	 * @pre the start location is already added.
	 * @note Only adds bezier points not end point.
	 */
	void AddSmallOrLargeBezierToPath(
		const FRotator& StartRotation,
		const FVector& BezierStart,
		const FVector& BezierEnd,
		const float CurveTension, const float AngleConsiderPointBehind);

	/**
	 * @pre the start location is already added.
	 * @note Only adds bezier points not end point.
	 * @param Distance: distance from bezier start to bezier end. note that bezier end is not added with this function to path
	 * only the between bezier points.
	 */
	void SetupLargeBezierMoveTo(
		const FVector& StartAirPoint,
		const FVector& EndAirPoint,
		const float CurveTension,
		const float Distance, const FVector& DirToTarget, int32 RegularBezierSampleAmount, const bool bIsToTheRight,
		const
		FVector& Forward
	);
	/**
	 * Samples a standard quadratic Bézier curve and appends the intermediate points.
	 * @param P0        Start point.
	 * @param P1        Control point.
	 * @param P2        End point.
	 * @param NumSamples Number of samples (excluding P0 and P2).
	 * @param OutPoints Array to append generated FAircraftPathPoint entries.
	 */
	static void AddQuadraticBezier(
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		int32 NumSamples,
		TArray<FAircraftPathPoint>& OutPoints);

	/**
		 * Samples a cubic Bézier curve for very large turns, using two control points:
		 *   - P1 controls the entry tangent from P0.
		 *   - P2 controls the exit tangent toward P3.
		 * This smooths out near-180° reversals without kinks.
		 *
		 * @param P0         Start point.
		 * @param P1         First control point (in front of P0).
		 * @param P2         Second control point (behind P3).
		 * @param P3         End point.
		 * @param NumSamples Number of samples (excluding P0 and P3).
		 * @param OutPoints  Array to append generated FAircraftPathPoint entries.
		 */
	static void AddCubicBezierLargeCurve(
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		const FVector& P3,
		int32 NumSamples,
		TArray<FAircraftPathPoint>& OutPoints);

	
	/**
	 * @brief Look for THREE consecutive points along the path where a direct attack dive is possible.
	 * If found at indices [i, i+1, i+2], we trim the path to end at the MIDDLE point (i+1).
	 * This makes the dive robust against small pose/position deviations when the next cycle starts.
	 *
	 * Facing used per point:
	 *  - i == 0        → StartRotation
	 *  - 0 < i < last  → look from point i to point i+1
	 *  - i == last     → look from point i-1 to point i
	 *
	 * @param StartRotation         Rotation at path start (used for i==0).
	 * @param EnemyAirLocation      Enemy location with any Z; Z is flattened to each point's altitude.
	 * @param AttackMoveSettings    Settings for GetIsWithinDirectAttackDiveRange.
	 *
	 * @post If a triple is found, truncates PathPoints to (middleIndex + 1).
	 *       Otherwise leaves PathPoints unchanged.
	 */

	void RemovePointsAfterAttackPossible(
		const FRotator& StartRotation,
		const FVector& EnemyAirLocation,
		const FAircraftAttackMoveSettings& AttackMoveSettings);

	/** 
      * After PathPoints has been filled, set each point’s Rotation:
      *  - [0] uses StartRotation
      *  - [Last] looks from StartAirPoint→EndAirPoint
      *  - each middle point “looks through” itself: finds a rotation
      *    from the previous location to the next location.
      *    @note If the point is dive or dive recovery we set the rotation to look at rotation.
      */
	void OnPathCompleteCalculateRotations(
		const FVector& EndAirPoint,
		const FRotator& StartRotation);

	//––– Stage 1: endpoints
	void SetInitialRoll(const FRotator& StartRotation);
	void SetFinalRoll(const FVector& EndAirPoint);

	//––– Stage 2: interior points
	void CalculateInteriorRolls();

	//––– Stage 3: Bezier‐curve helpers
	float GetBezierRollBonus(int32 Index) const;
	void GatherBezierNeighbors(int32 Index, TArray<FVector>& OutPrev, TArray<FVector>& OutNext) const;
	float GetExtraBezierRoll(const FVector& Curr, const TArray<FVector>& Prev, const TArray<FVector>& Next,
	                         float BonusSign) const;

	//––– Stage 4: straight‐line blending
	float GetBlendedRoll(const FVector& Prev, const FVector& Curr, const FVector& Next) const;

	float GetRollToAddToBezierPoint(
		const FVector& Curr,
		const FVector& Next,
		const FRotator& StartRotation) const;

	FVector GetBezierStartOutOfDeadZone(
		const FVector& StartAirPoint,
		const FVector& EndAirPoint,
		const FRotator& StartRotation,
		const float AngleToFirstPoint, const FAircraftDeadZone& DeadZoneSettings);
};
