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
