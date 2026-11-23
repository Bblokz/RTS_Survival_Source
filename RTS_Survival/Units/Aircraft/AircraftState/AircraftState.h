#pragma once

#include "CoreMinimal.h"

#include "AircraftState.generated.h"

UENUM()
enum class EAircraftLandingState : uint8
{
	None,
	Landed,
	VerticalTakeOff,
	Airborne,
	WaitForBayToOpen,
	VerticalLanding,
};

UENUM()
enum class EAircraftMovementState
{
	None,
	Idle,
	MovingTo,
	AttackMove,
};


UENUM()
enum class EAircraftAirborneBehavior : uint8
{
	None,
	Hover,
	InCircles
};

// Both angle to and distance to the point need to be within the deadzone for the plane to take a different path.
USTRUCT(Blueprintable, BlueprintType)
struct FAircraftDeadZone
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float AngleAtDeadZone = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float DistanceAtDeadZone = 800.f;
};

USTRUCT(Blueprintable, BlueprintType)
struct FAircraftBezierCurveSettings
{
	GENERATED_BODY()
	/** 
	 * Controls how far out the Bézier control point is placed along the start-to-end vector:
	 *
	 * - 0.5 (default): midpoint control point → balanced arc  
	 * - <0.5          : control point closer to start → sharper turn  
	 * - >0.5          : control point farther out  → gentler, longer curve  
	 *
	 * Valid range [0.0 … 1.0].  
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (
			ClampMin = "0.0", ClampMax = "1.0",
			UIMin = "0.0", UIMax = "1.0"
		))
	float BezierCurveTension = 0.5f;

	/** 
	 * If the angle between the aircraft’s forward direction and the target exceeds this value (in degrees),
	 * the path generator will use a “large-curve” Bézier to smooth out the turn.
	 *
	 * Valid range [0.0 … 180.0].
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (
			ClampMin = "0.0", ClampMax = "180.0",
			UIMin = "0.0", UIMax = "180.0"
		))
	float AngleConsiderPointBehind = 75.f;

	// Helps the aircraft roll extra when flying along a bezier curve.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (
			ClampMin = "0.25", ClampMax = "10",
			UIMin = "0.25", UIMax = "10"
		))
	float BezierCurvePointRollMlt = 1.2f;


	// If the move to location exceeds this distance we use the bezier till MaxBezierDistance and then switch to a straight line.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (
			ClampMin = "1000", ClampMax = "10000"))
	float MaxBezierDistance = 5000.f;
};


USTRUCT(Blueprintable, BlueprintType)
struct FAircraftAttackMoveSettings
{
	GENERATED_BODY()

	// the absolute angle in degrees for which we allow to start an attack dive.
	// we check if the abs angle to target is within this range.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (
			ClampMin = "0.0", ClampMax = "180.0",
			UIMin = "0.0", UIMax = "180.0"
		))
	float AngleAllowAttackDive = 75.f;

	// The range (min, max) of distances at which a direct attack dive is allowed.
	//   X = minimum dive distance: 500–2000
	//   Y = maximum dive distance: 500–4000
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (
			ClampMinX = "500.0", ClampMaxX = "2000.0",
			UIMinX = "500.0", UIMaxX = "2000.0",
			ClampMinY = "500.0", ClampMaxY = "10000.0",
			UIMinY = "500.0", UIMaxY = "10000.0"
		)
	)
	FVector2D AttackDiveRange = FVector2D(800.f, 7000.f);


	// Adjusts how fast we start the recovery maneuver of the attack dive.
	// Set this to large values to increase the bomb run-way time.
	// Increasing this value will decrease the time spend in the dive of the attack.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (
			ClampMin = "1.0",
			UIMin = "1.0"
		))
	float AttackDive_RecoveryStrength = 1.f;


	// Increases the length of the recovery part of the attack dive.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (
			ClampMin = "1.0",
			UIMin = "1.0"
		))
	float AttackDive_RecoveryLengthAddition = 100.f;

	// adjusts the dive part of the attack dive, increase this value to make the time the plane can uses the wing
	// weapons in the attack dive longer.
	// Increasing this value will decrease bomb run-way time.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (
			ClampMin = "1.0",
			UIMin = "1.0"
		))
	float AttackDive_DiveStrength = 3.f;

	// how much we can maximally drop below our base height to perform the attack dive.
	// Should not be higher than TakeOffHeight
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (
			ClampMin = "100.0"
		))
	float DeltaAttackHeight = 500.f;

	// The dive never bottoms out lower than this above the target, whatever DeltaAttackHeight asks for (cm); a target
	// on higher ground than the dive's start then gets a shallower dive instead of one into the hill.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinDiveHeightAboveTarget = 1000.f;

	// When the angle to the enemy is too extreme:
	// Used to determine the final U-turn point to the enemy, make this bigger for larger turns.
	// gets multiplied with the angle/90.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (
			ClampMin = "250", ClampMax = "5000",
			UIMin = "250", UIMax = "5000"
		))
	float StartHzOffsetDistanceUTurnToEnemy = 2000.f;

	// Used to add to offset distance when the angle is extreme.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (
			ClampMin = "0", ClampMax = "2000",
			UIMin = "0", UIMax = "2000"
		))
	float UTurnExtremeAngleOffset = 100.f;
};


/**
 * Keeps an airborne aircraft at the ground clearance it had when its flight height was set, over hills and valleys.
 * Not applied while it dives, drops bombs or flies a rope-slung payload in; those keep their planned heights.
 */
USTRUCT(Blueprintable, BlueprintType)
struct FAircraftTerrainFollowSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bEnabled = true;

	// Seconds between two ground samples; between samples the height eases every frame toward the last one.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.05", UIMin = "0.05"))
	float SampleInterval = 0.3f;

	// The ground is also sampled this many seconds of flight ahead (at cruise speed at least), so the aircraft climbs
	// before a hill instead of on it, and only descends once the valley lies under the whole look-ahead. Tuned on the
	// Ore Mountains ridges: a fast aircraft needs about this long to climb a 18 m rise at the speed caps below.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LookAheadSeconds = 3.5f;

	// Fastest climb or descent caused by the terrain (cm/s).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxVerticalSpeed = 700.f;

	// How quickly a climb or descent starts and stops (cm/s²); lower is softer.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxVerticalAcceleration = 500.f;

	// The aircraft never follows the ground lower than this above it, whatever height it started at (cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinGroundClearance = 300.f;

	// Regular cruise flight (after a take-off or when a move or attack sets the cruise height) keeps at least this much
	// above the ground; a lower take-off height climbs to it smoothly once airborne. Ability flights that pick their own
	// height above their target are not raised by it (cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinCruiseClearance = 2200.f;

	// A level aircraft ignores height changes smaller than this, so uneven ground does not make it bob (cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HeightDeadZone = 40.f;

	// Largest nose up / down angle shown while the terrain makes the aircraft climb or descend (degrees).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "45.0", UIMin = "0.0",
		UIMax = "45.0"))
	float MaxClimbPitch = 15.f;
};

/**
 * Bank-to-turn flight of move, attack and idle flights (FAircraftFlightModel): the flight direction turns toward a point
 * a little ahead on the path at a capped, smoothly changing rate, the speed is kept through turns, and the nose and the
 * bank follow the flight direction. Off (or -AircraftLegacyFlight) flies the old homing on path points.
 */
USTRUCT(Blueprintable, BlueprintType)
struct FAircraftFlightModelSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bEnabled = true;

	// Fastest heading change (deg/s); with the cruise speed it sets the tightest turn: radius = speed / rate.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "5.0", UIMin = "5.0"))
	float MaxTurnRate = 90.f;

	// How fast the turn rate builds up and eases off (deg/s²); lower rolls into and out of turns more softly.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "10.0", UIMin = "10.0"))
	float TurnRateAcceleration = 180.f;

	// Turn and climb rate per degree still to turn (1/s); higher settles on a new heading sooner, too high wobbles.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.5", UIMin = "0.5"))
	float TurnResponsiveness = 2.f;

	// Fastest change of the climb / dive angle (deg/s) and how fast that rate builds up (deg/s²).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "5.0", UIMin = "5.0"))
	float MaxPitchRate = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "10.0", UIMin = "10.0"))
	float PitchRateAcceleration = 240.f;

	// Steepest climb or dive of the flight direction (degrees).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "5.0", ClampMax = "80.0", UIMin = "5.0",
		UIMax = "80.0"))
	float MaxFlightPitch = 50.f;

	// Bank at the fastest turn (degrees); a gentler turn banks proportionally less.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "85.0", UIMin = "0.0",
		UIMax = "85.0"))
	float MaxBankAngle = 60.f;

	// How quickly the bank follows the turn (1/s), and the fastest the bank may change (deg/s).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.5", UIMin = "0.5"))
	float BankInterpSpeed = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "5.0", UIMin = "5.0"))
	float MaxRollRate = 90.f;

	// Cruise and idle: climbs and descents to the path height ease in and out at these limits instead of pointing the
	// nose at a point ahead (which ballooned past cruise height after every pull-out).
	// Vertical speed per cm of height error (1/s).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.1", UIMin = "0.1"))
	float AltitudeHoldGain = 1.2f;

	// How hard a climb or descent may ease out (cm/s²).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "10.0", UIMin = "10.0"))
	float AltitudeHoldAcceleration = 300.f;

	// Steepest climb or descent of the altitude hold (degrees).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1.0", ClampMax = "45.0", UIMin = "1.0",
		UIMax = "45.0"))
	float MaxAltitudeHoldPitch = 20.f;

	// Paths are planned as turns at this share of MaxTurnRate at cruise speed, so the flight has turn rate in reserve
	// to correct; the planned turn radius is cruise speed / (share x MaxTurnRate).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.2", ClampMax = "1.0", UIMin = "0.2",
		UIMax = "1.0"))
	float PlanningTurnRateShare = 0.75f;

	// The aircraft aims at the point this many seconds of flight ahead along its path; longer rounds corners more.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.05", UIMin = "0.05"))
	float PathLookAheadSeconds = 0.6f;

	// The look-ahead is at least this share of the tightest turn radius at the current speed; much shorter makes the
	// aircraft overshoot the path and bank back.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LookAheadTurnRadiusShare = 0.8f;

	// Look-ahead into and out of a dive (seconds of flight): short, so the dive reaches its planned depth.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.05", UIMin = "0.05"))
	float DiveLookAheadSeconds = 0.3f;

	// Bounds of that look-ahead distance (cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "50.0", UIMin = "50.0"))
	float MinPathLookAhead = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "50.0", UIMin = "50.0"))
	float MaxPathLookAhead = 4000.f;

	// Share of the speed given up in the hardest turn (0 keeps the full speed); scales with the turn rate squared.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "0.5", UIMin = "0.0",
		UIMax = "0.5"))
	float TurnSpeedLoss = 0.08f;

	// How fast the speed falls toward a lower wanted speed, such as the idle speed (cm/s²).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1.0", UIMin = "1.0"))
	float SpeedDecreaseRate = 250.f;

	// Flights that end in a hover (return to base, drops) slow down on the way in at this rate (cm/s²) to this speed
	// (cm/s), so the hover does not start from cruise speed with a jolt.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "10.0", UIMin = "10.0"))
	float ArrivalDeceleration = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "10.0", UIMin = "10.0"))
	float MinArrivalSpeed = 250.f;

	// A dive descends at most as fast as reaches its floor (target height + part of MinDiveHeightAboveTarget) in
	// this many seconds, so it flattens out smoothly before the floor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.1", UIMin = "0.1"))
	float FloorReactionSeconds = 0.5f;
};

USTRUCT(Blueprintable, BlueprintType)
struct FAircraftMovementData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debugging")
	bool bDebugState = false;
	
	// Whether this aircraft started in landed position.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Idle")
	bool bM_StartedAsLanded = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Idle")
	EAircraftAirborneBehavior AirborneBehavior = EAircraftAirborneBehavior::InCircles;

	// The radius of the circle the aircraft flies in when airborne idle.
	// Not used if the aircraft can hover.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Idle")
	float IdleRadius = 4000.f;

	/** Bank (roll) angle in degrees when circling idle
	 * also used to determine the pich recovery angle the aircraft can take.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Idle")
	float IdleBankAngle = 15.f;

	/** Speed at which the aircraft roll interpolates toward its target bank (deg/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Idle")
	float BankInterpSpeed = 2.f;

	// Multiplied with the MaxMoveSpeed to determine the movement speed when in idle circle.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Idle")
	float IdleMovementSpeedMlt = 0.66f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float Acceleration = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float Deceleration = 10.f;

	/** Max horizontal speed when flying to a move-command target (units/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	float MaxMoveSpeed = 2000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	float PathPointAcceptanceRadius = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	FAircraftBezierCurveSettings BezierCurveSettings;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FAircraftAttackMoveSettings AttackMoveSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	FAircraftTerrainFollowSettings TerrainFollowSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	FAircraftFlightModelSettings FlightModelSettings;

	// If both conditions hold we consider the point in deadzone and have the aircraft fly away before
	// turning.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement")
	FAircraftDeadZone DeadZoneSettings = FAircraftDeadZone();

	// How long the aircraft prepares VTO on the ground until actual liftoff.
	// If set to zero the aircraft instantly takes off.
	// This will also be used for landing; how long the aircraft hovers before starting the landing.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VTO")
	float VtoPrepareTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VTO")
	float VerticalAcceleration = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VTO")
	float MaxVerticalTakeOffSpeed = 300.f;

	// Height between landed position and airborne position.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VTO")
	float TakeOffHeight = 800.f;
	
	// How much Z value the aircraft goes up and down from its starting position when it is hover idle.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoverIdle")
	float IdleHoverDeltaZ = 50.f;

	// Scales how fast the aircraft goes up and down when hover idle.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoverIdle")
	float IdleHoverSpeedMlt = 1.f;
};
