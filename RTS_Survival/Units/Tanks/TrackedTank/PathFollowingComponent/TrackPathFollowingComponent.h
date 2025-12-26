// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "RTS_Survival/Units/Tanks/VehicleAI/Components/VehiclePathFollowingComponent.h"
#include "TrackPathFollowingComponent.generated.h"


class UTrackPhysicsMovement;
class ATrackedTankMaster;

/**
 * @brief Captures overlap tracking data for allied actors while path following.
 */
USTRUCT()
struct FOverlapActorData
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY()
	float TimeRegisteredSeconds = 0.f;
};

/* Enumeration defining the speed units used for speed control */
UENUM()
enum ESpeedUnits
{
	MilesPerHour,
	KilometersPerHour,
	CentimetersPerSecond
};

UENUM()
enum E_CrowdSimulationState
{
	Enabled,
	ObstacleOnly,
	Disabled,
};

UENUM()
enum class EEvasionAction
{
	MoveNormally,
	Wait,
	EvadeRight
};

/**
 * @note common navigation issues:
 * @note 0) jittering of speed; Increase throttle threshold.
 * @note 1) Vehicle goes into reverse too easily; increase should reverse angle and decrease reverse max distance.
 */
/**
 * @brief Vehicle path following component for tracked tanks with overlap-aware evasion logic.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTrackPathFollowingComponent : public UVehiclePathFollowingComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTrackPathFollowingComponent();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	/**
	 * @brief Register an overlapped actor that currently blocks this vehicle's movement.
	 * @param InActor Actor we are overlapping with (idle ally pushed aside by evasion).
	 */
	void RegisterIdleBlockingActor(AActor* InActor);

	void RegisterMovingOverlapBlockingActor(AActor* InActor);
	/**
	 * @brief Returns true if any valid blocking overlaps are currently registered.
	 * @return true when at least one valid actor is still considered blocking.
	 */

	void DeregisterOverlapBlockingActor(AActor* InActor);
	bool HasBlockingOverlaps() const;

	/************/
	/* Reverse */

	/**
	* Set this agent to force reverse manually
	* @param Reverse - Toggle the force reverse
	*/
	UFUNCTION(BlueprintCallable, Category = "Vehicle AI|Reverse")
	void SetReverse(bool Reverse);


	/**
	* Toggles the automatic reverse capability
	* @param Reverse - Toggle the automatic reverse
	*/
	UFUNCTION(BlueprintPure, Category = "Vehicle AI|Reverse")
	bool IsReversing();

	UFUNCTION(BlueprintCallable)
	void SetTrackPhysicsComponent(UTrackPhysicsMovement* TrackPhysicsMovement)
	{
		M_TrackPhysicsMovement = TrackPhysicsMovement;
	}

	inline float GetGoalAcceptanceRadius() const { return VehicleGoalAcceptanceRadius; }


	/* Returns the agents controlled pawn */
	APawn* GetValidControlledPawn();


	void SetDesiredReverseSpeed(const float NewSpeed, ESpeedUnits SpeedUnit);

	/**
    * Updates this agents desired speed
    * @param NewSpeed - Defines the new speed, given in Unreal Units (cm/s)
    * @param Unit - The units of data to use for the speed conversion which is done internally
    */
	UFUNCTION(BlueprintCallable, Category = "Vehicle AI|Speed Control")
	void SetDesiredSpeed(float NewSpeed, ESpeedUnits Unit);

	// Used by technologies to access vehicle speed.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Alter Vehicle Speed")
	inline float BP_GetDesiredSpeed() const { return DesiredSpeed; }


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Alter vehicle speed")
	void BP_SetNewVehicleSpeed(float NewSpeed, ESpeedUnits Unit);


	/**
	* Checks if this vehicle is stuck (on path but hasn't moved in a while according to sample data)
	* @return true if the vehicle is stuck
	*/
	UFUNCTION(BlueprintPure, Category = "Vehicle AI|Stuck Detection")
	bool IsStuck(const float DeltaTime);

	void ClearOverlapsForNewMovementCommand();
	

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void BeginDestroy() override;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual void OnPathUpdated();


	/**
	 * @brief Updates the vehicle steering and throttle calculations using the interface.
	 *
	 * @note Throttle: Is adjusted with PID depending on the speed difference between the current and desired speed.
	 * @note The Desired speed is adjusted with the function TrackGetSlowdownSpeed.
	 * @note ********************************************************************************
	 * @note With debugging this will lead to three types of lines:
	 * @note 1) Purple/Orange: The vehicle adjusts speed as within SlowdownDistance from final destination.
	 * @note Becomes orange when on an incline then the desired speed is NOT adjusted.
	 * @note Tweaking: SlowdownDistance to a higher values will cause earlier slowdown the final destination.
	 * @note ********************************************************************************
	 * @note 2) Red: The vehicle is adjusting the desired speed because the current angle needed for future path points is too sharp.
	 * @note Tweaking: Increasing the MaxAngleDontSlow will cause the vehicle to ignore more angles.
	 * @note ############################################
	 * @note Tweaking CornerSpeedControlPercentage: This value scales the maximum speed derived from cornering constraints.
	 * Increasing this percentage leads to higher allowed speeds for cornering,
	 * making the vehicle potentially more susceptible to loss of control. Decreasing it results in safer, slower cornering speeds.
	 * @note ############################################
	 * @note Tweaking VehicleMass: In the formula to determine maximum cornering speed,
	 * increasing the vehicle mass decreases the resultant speed
	 * @note ********************************************************************************
	 * @note 3) Green: The vehicle is maintaining the desired speed.
	 * @note ********************************************************************************
	 * @note PID: Adjustst the throttle based on the speed difference.
	 * @note Tweaking: DesiredSpeedThrottleThreshold determines how sensitive the throttle is to the speed difference.
	 * Generally leave this at a high value, for Panther G it is 1800.
	 * @note ********************************************************************************
	 * @note Steering: Will always steer maximally if the angle is greater than MaxAngleDontSlow.
	 * @note if <MaxAngleDontSlow it Adjusts steering down in ratio to the Angle.
	 * @note ********************************************************************************
	 * @note Reverse: The vehicle will reverse to the destination if the angle is greater than the ShouldReverseAngle.
	 * @note Tweaking: ShouldReverseAngle determines the angle at which the vehicle will reverse.
	 * Reverse Max Distance determines the distance at which the vehicle will reverse even if the angle was within the
	 * ShouldReverseAngle.
	 * @note Use Reverse Speed Threshold to adjust how sensitive the PID is when reversing (generally left lower).
	 * @note use DesiredReverseSpeedKmph to set the desired reverse speed.
	 * @note ********************************************************************************
	 * @note Deadzones: The vehicle will kill the throttle if the angle is greater than the deadzone angle
	 * and the distance is less than the deadzone distance. In this case the vehicle will only steer.
	 * @note ********************************************************************************
	 * @param Destination The current destinnation on the path of the vehicle.
	 * @param DeltaTime The between past two frames.
	 */
	void UpdateDriving(FVector Destination, float DeltaTime);

	bool CheckOverlapIdleAllies(float DeltaTime);
	bool CheckOverlapMovingAllies(float DeltaTime);


	/* The speed of this vehicle in cm/s, speed is always converted into cm/s in the plugin as this is the default velocity units
	This value changes as the Starting Desired Speed changes and/or its units change */
	UPROPERTY(VisibleAnywhere, Category = "Speed Control")
	float DesiredSpeed = 1000.f;

	/* The starting desired speed of this vehicle, the units of which are defined by the Start Speed Unit */
	UPROPERTY(EditDefaultsOnly, Category = "Speed Control")
	float StartingDesiredSpeed = 1000.f;

	/* The speed units this vehicle will use when the desired speed */
	UPROPERTY(EditDefaultsOnly, Category = "Speed Control")
	TEnumAsByte<ESpeedUnits> StartSpeedUnit = ESpeedUnits::CentimetersPerSecond;


	/**
	 * @brief Whether to use throttle PID control.
	 *
	 * The PID controller saves samples of the difference between the DesiredSpeed adjusted for destination distance(!)
	 * and the current speed on the y-axis and the deltatime of the frame on the x-axis.
	 * @note Uses DesiredSpeedThrottleThreshold to normalise the difference between the current speed and the desired speed.
	 * @note ****************************************************************
	 * @note Why Delta Time Matters: Even though DeltaTime is small (around 0.016 seconds per frame),
	 * it is essential for ensuring that the PID calculations are frame-rate independent.
	 * Without incorporating DeltaTime, the vehicle’s behavior could vary significantly with different frame rates.
	 * @note ****************  (P)roportional Calculation  ****************
	 * @note P-coefficient * the most recent error: this is directly influenced by (the latest speed difference).
	 * @note ****************  (I)ntegral Calculation  ****************
	 * @note Integrating over DeltaTime means you're summing up all the past errors,
	 * each weighted by the time interval over which it occurred.
	 * This prevents the integral term from becoming disproportionately large in high frame rate scenarios where DeltaTime is smaller.
	 * @note ****************  (D)erivative Calculation  ****************
	 * Derivative is calculated as the change in error divided by the change in time (DeltaTime).
	 * This gives a rate of change of the error, which helps in predicting and damping the future behavior of the error.
	 */


	/* Sets up each coefficient for the throttle PID controller
	*  P: Proportional to the amount needed to speed up, generally fine left as 1
	*  I: Adjust this value if the throttle keeps oscillating (increase in small intervals 0.1)
	*  D: Generally not needed, but increase if I doesn't fix the oscillating
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Speed Control")
	FPIDCoefficients ThrottlePIDSetup = FPIDCoefficients(1.f, 0.1f, 0.f);

	/* The PID controller used for the Throttle */
	FPIDController ThrottlePID;


	/**
	 * This value is used to compare the current speed to the Adjusted desired speed for stopping distance.
	 *
	 * @note ********************************************************************
	 * @note Lowering this Threshold:
	 * @note Makes the controller more sensitive to smaller differences in speed.
	 * @note Causes the SpeedDifference to reach its maximum/minimum values (±1) more quickly as smaller deviations from the desired speed are magnified.
	 * Which can lead to a more aggressive response from the PID controller, potentially causing overshoot or oscillation
	 * if not balanced with appropriate PID coefficients.
	 * @note ********************************************************************
	 * @note Raise this Threshold:
	 * @note Makes the controller less sensitive to speed differences.
	 * @note Smaller deviattions are normalized to a smaller range of speed difference, leaading to a smoother, potentially slower response.
	 * This can be beneficial for maintaining stability and preventing excessive throttle adjustments.
	 * @note ********************************************************************
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Speed Control|Throttle")
	float DesiredSpeedThrottleThreshold = 300.f;


	/** This is the value that the speed control scales by when using the advanced control, 1 means it goes the max speed it physically can around a corner
	*  (within the desired speed) while maintaining more precise contorl, you can make this higher than 1 if precise path following isn't necessary.
	* 
	*  Tweaking:
	* 
	*  Vehicle doesn't follow corner precisely enough: Reduce
	*  Vehicle goes around corners too slowly: Increase (until it starts to not handle very well)
	*
	*  @note: Affects vehicle max speed very much!
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Speed Control|Advanced", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CornerSpeedControlPercentage = 1.f;


	/**********/
	/* Debug */
	/********/

	/* Shows debug data on screen for this agent */
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bDebug = false;

	// Shows prints related to acceptance radias
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bDebugAcceptanceRadius = false;

	// Shows prints related to slowing down distance and braking
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bDebugSlowDown = false;

	/* The maximum speed this vehicle is able to get to. Used by crowd following for collision sampling */
	UPROPERTY(EditDefaultsOnly, Category = "Crowd Following|Advanced")
	float VehicleMaxSpeed = 1000.f;


	/********************************************/
	/* BEGIN Path Following Component Overrides 
	/******************************************/

	/** follow current path segment */
	virtual void FollowPathSegment(float DeltaTime) override;

	/** check state of path following, update move segment if needed */
	virtual void UpdatePathSegment() override;

	/** sets properties relating to path following */
	virtual void SetMoveSegment(int32 SegmentStartIndex) override;

	/** notify about finished movement */
	virtual void OnPathFinished(const FPathFollowingResult& Result) override;
	void UpdateVehicle(float Throttle, float CurrentSpeed, float DeltaTime, float BreakAmount, float Steering);

	/** Initalize all of the variables */
	virtual void Initialize() override;
	bool DoesControlledPawnImplementInterface();

	virtual FVector GetMoveFocus(bool bAllowStrafe) const override;

	virtual void OnPathfindingQuery(FPathFindingQuery& Query) override;


	/********************************************/
	/* END Path Following Component Overrides 
	/******************************************/

	/* The acceptance radius for an individual path point */
	UPROPERTY(EditDefaultsOnly, Category = "Path Following|Advanced")
	float VehiclePathPointAcceptanceRadius = 50.f;


	/* The acceptance radius used on final destination */
	UPROPERTY(EditDefaultsOnly, Category = "Path Following|Advanced")
	float VehicleGoalAcceptanceRadius = 50.f;

	/* The multiplier used against the vehicle radius to check if the vehicle has reached final destination.
	 * default 1.1*/
	UPROPERTY(EditDefaultsOnly, Category = "Path Following|Advanced")
	float DestinationVehicleRadiusMultiplier = 1.1f;


	// stoppin distance calculations.

	/* The mass of the vehicle being used, can be changed if using a different vehicle setup */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Physics")
	float VehicleMass = 1500.f;

	/* The mass of the vehicle being used, can be changed if using a different vehicle setup */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Physics")
	float VehicleDownforce = 0.3f;


	/* The world gravity, if you use a custom gravity setting you can change this here */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Physics")
	float WorldGravity = 980.f;


	/********************/
	/* Slowdown System */
	/******************/

	/** The distance at which to start the slowdown, the greater this is, the smoother the slowdown is,
	   especially at higher speeds
	*/
	UPROPERTY(EditAnywhere, Category = "Slowdown|Advanced")
	float SlowdownDistance = 2000.f;

	/* The number of samples taken to calculate the speed at which to slow down
	   they are only included if within the slowdown distance. This helps a vehicle get
	   around a corner which could be made up of multiple path points, close together. This is mainly here for performance reasons*/
	UPROPERTY(EditAnywhere, Category = "Slowdown|Advanced")
	int CornerSlowdownSamples = 8;


	/***************************/
	/* Stuck Detection System */
	/*************************/

	/** 
     * @brief Scales how long the vehicle is allowed to be at low speed before considering itself stuck.
     *        Higher values → vehicle waits longer before considering itself stuck due to low speed
     *        adjust if the vehicle has very extreme deadzones -> a lot of stationary turning. 
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stuck Detection")
    float StuckTimeLowSpeedScale = 1.0f;

	/* Is this vehicle currently stuck and unable to move? */
	bool bIsStuck = false;

	/* Amount of samples in an array to use for stuck detection */
	UPROPERTY(EditDefaultsOnly, Category = "Stuck Detection|Advanced")
	int StuckDetectionSampleCount = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Stuck Detection|Teleport")
	float TeleportDistance = 50.f;

	/* used to determine the next index of the array to populate with a stuck location */
	int NextStuckSampleIdx = 0;

	/* Sample of vectors used to decide if the agent is stuck */
	TArray<FVector> StuckLocationSamples;

	/* The last time a sample of the location was taken */
	float LastStuckSampleTime;

	/* Enables detecting when this agent hasn't moved in a while, then executing the UnStuck code */
	UPROPERTY(EditDefaultsOnly, Category = "Stuck Detection")
	bool bUseStuckDetection = true;


	/* The location when we're stuck we want to move to */
	FVector UnStuckDestination;

	/*********************/
	/* Reverse Handling */
	/*******************/

	/**
     * @brief If the total remaining path length to the final goal is greater than this distance (cm),
     *        the vehicle will first rotate in place to face the first/next path point instead of
     *        attempting to reverse because of a large initial heading error. This avoids
     *        unintentional long reverse paths.
     * @note  Set to 0 to disable (default). Uses DeadZoneAngle as the “aligned” tolerance.
     */
	UPROPERTY(EditDefaultsOnly, Category = "Reverse|Advanced", meta=(ClampMin="0.0"))
	float FaceFirstPointIfEndDistanceGreaterThan = 2000.f;

	/* If the angle to the target is greater than this, the vehicle will reverse to the target point */
	UPROPERTY(EditDefaultsOnly, Category = "Reverse|Advanced", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ShouldReverseAngle = 140.f;

	/* If the distance is lower than this then the vehicle will reverse to the target point */
	UPROPERTY(EditDefaultsOnly, Category = "Reverse|Advanced")
	float ReverseMaxDistance = 1000.f;

	/*
	 * If the vehicle is already reversing (bReversing),
	 * it checks whether the TurnAngle is greater than a slightly reduced reverse angle (ShouldReverseAngle - ReverseThreshold).
	 * This hysteresis (using ReverseThreshold)
	 * prevents the control system from oscillating between moving forward and reversing when the vehicle is close to the threshold angle.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Reverse|Advanced", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ReverseThreshold = 40.f;


	/* Distance to try and move when stuck and the left and right traces are blocked,
	 * we use this distance to find a point behind the vehicle to move to*/
	UPROPERTY(EditDefaultsOnly, Category = "Stuck Detection|Advanced")
	float UnStuckDistance = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Stuck Detection|Advanced")
	float UnStuckTraceDistance = 300.f;


	/* Distance at which the agent will be classified as stuck */
	UPROPERTY(EditDefaultsOnly, Category = "Stuck Detection|Advanced")
	float StuckDetectionDistance = 100.f;

	/* Distance at which to stop and recheck if stuck */
	UPROPERTY(EditDefaultsOnly, Category = "Stuck Detection|Advanced")
	float StuckAcceptanceRadius = 50.f;

	/* Time interval when to add to the stuck sample array */
	UPROPERTY(EditDefaultsOnly, Category = "Stuck Detection|Advanced")
	float StuckDetectionInterval = 0.5f;

	/****************/
	/* Deadonzes*/
	/**************/

	/**
	 * @brief Distance in cm for which we will start checking if path points' their angles are too sharp to reach.
	* @note For TrackedVehicles: stationary turn ; so we set the throttle to 0.
	* @note For WheeledVehicles: inject reverse move focus.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Deadzones")
	float DeadZoneDistance;


	/**
	 * Larger DeadZoneAngle:
	* Requires a Greater Angle Deviation to enter the deadzone.
	* Effect: The vehicle needs to be more precisely aligned with the destination before halting throttle input.
	* Result: Less Easy to enter the deadzone; the vehicle remains active longer.*/
	UPROPERTY(EditDefaultsOnly, Category = "Deadzones")
	float DeadZoneAngle;

	// The point to reverse to when the deadzone is entered; only for wheeled vehicles.
	UPROPERTY(EditDefaultsOnly, Category = "Deadzones")
	float DeadZoneReverseDistance = 600;


private:
	/**
	 * @brief If the angle to the current path point is smaller than this the tank will not slow down.
	 * @note Will slow down if the current path point == final destination and we are within slowdown distance.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TrackPathFollowing")
	float MaxAngleDontSlow;

	// Used to slowdown to final destination. Set to 1/5 of desired speed.
	float M_MinimalSlowdownSpeed;

	// Speed fed into PID throttle controller to adjust the throttle in cm/s.
	// Do not set this too high as we do not adjust the reverse speed for corners.
	UPROPERTY(VisibleDefaultsOnly, Category = "TrackPathFollowing")
	float DesiredReverseSpeed = 277.778f;

	// Do not set this too high as we do not adjust the reverse speed for corners.
	// Only used to set the DesiredReverseSpeed.
	UPROPERTY(EditDefaultsOnly, Category = "Speed Control")
	float DesiredReverseSpeedKmph = 10.f;


	float SmoothedTargetAngle = 0;
	float SmoothedDestinationDistance = 0;

	// Smoothing factor (alpha value between 0 and 1)
	const float SmoothingFactor = 0.1f;

	// Actors we are temporarily waiting for because of footprint evasion on idle allies.
	UPROPERTY()
	TArray<FOverlapActorData> M_IdleAlliedBlockingActors;
	// Allied actors that are moving and we need to resolve collision with.
	UPROPERTY()
	TArray<FOverlapActorData> M_MovingBlockingOverlapActors;

	static int32 M_TicksCountCheckOverlappers;
	static float M_TimeTillDiscardOverlap;

	void DebugIdleOverlappingActors();
	void DebugMovingOverlappingActors(const float DeltaTime);

	// Last steering command applied to the vehicle; reused while paused for overlaps.
	float M_LastSteeringInput = 0.f;
	float M_LastThrottleInput = 0.f;

	/**
	 * In general, leave this (a lot) lower than the threshold used for forward speed as the reverse speed is way lower.
	 * @note ********************************************************************
	 * @note Lowering this Threshold:
	 * @note Makes the controller more sensitive to smaller differences in speed.
	 * @note ********************************************************************
	 * @note Raise this Threshold:
	 * @note Makes the controller less sensitive to speed differences.
	 * @note ********************************************************************
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TrackPathFollowing")
	float ReverseSpeedPIDThreshold = 100.f;

	void DrawNavLinks(const FColor Color);
	float CalculateDestinationAngle(const FVector& Destination);
	float CalculateDestinationDistance(const FVector& Destination);
	float GetPathLengthRemaining();
	FVector GetUnstuckLocation(const FVector& MoveFocus);
	bool VisibilityTrace(const FVector& Start, const FVector& End) const;
	FVector TeleportOrCalculateUnstuckDestination(const FVector& MoveFocus);
	FVector FindTeleportLocation();
	void ResetStuck();


	/**
	 * @brief Calculates the speed that is desired depending on the distance to the final desination,
	 * The angle to the next points and distances to next path points.
	 * @param DistanceToNextPoint Distance to next path point.
	 * @param Destination The current path point to aim for.
	 * @return The desired speed based on the current situation.
	 */
	float TrackGetSlowdownSpeed(
		const float DistanceToNextPoint,
		const FVector& Destination);

	/**
	 * @brief Calculates the maximum allowable speed for a vehicle on a track based on path curvature and stopping distances.
	 *
	 * This function evaluates the vehicle's path points to determine the maximum safe cornering speed by calculating
	 * the cumulative curve radius of the path and the associated angle changes between path segments. It uses
	 * the calculated stopping distance for safety and performance considerations in speed adjustment.
	 *
	 * @return float The calculated maximum speed that the vehicle can safely travel at, considering the current path's curvature.
	 *
	 * @note The following variables can be adjusted to alter the behavior of this function:
	 * @note `DesiredSpeed`: Affects the initial assumption of maximum cornering speed.
	 * @note `CornerSlowdownSamples`: Influences the number of path samples considered for curve radius calculation.
	 * @note `StoppingDistance`: Determines how early the vehicle should begin to slow down for curves.
	 * @note This function uses `CalculateTrackStoppingDistance()` to determine dynamic stopping distances based on current velocity.
	*/
	float CalculateTrackMaximumSpeedWithSamples();
	float GetMaximumCorneringSpeed(float CurveRadius);

	FORCEINLINE float CalculateTrackStoppingDistance(float Velocity);

	bool IsPathClear(FVector Start, FVector End);

	float M_TimeWantDesiredSpeed;

	float M_CurrentSpeed;

	UPROPERTY(EditDefaultsOnly, Category = "TrackPathFollowing")
	float M_ThrottleIncreaseForDesiredSpeed = 0.2;

	UPROPERTY(editDefaultsOnly, Category = "TrackPathFollowing")
	float M_ReverseThrottleIncreaseForDesiredSpeed = 0.2;

	/**
	 * Checks whether the vehicle should reverse, allows for manual revers if bWantsReverse is set to true.
	 * @param TurnAngle In []
	 * @param TargetDistance 
	 * @return 
	 */
	bool ShouldTrackVehicleReverse(const float TurnAngle, const float TargetDistance);
	void RegisterWithRTSNavigator();
	bool UpdateStuckSamples();
	float CalculateAverageAngleWithSamples(float& OutDistance);
	float CalculateLargestAngleWithSamples(float& OutDistance);

	void ResetOverlapBlockingActors();
	void RemoveExpiredOverlaps(
		const float CurrentTimeSeconds,
		TArray<FOverlapActorData>& TargetArray);
	void RemoveInvalidOverlaps(TArray<FOverlapActorData>& TargetArray) const;
	void AddOverlapActorData(
		TArray<FOverlapActorData>& TargetArray,
		AActor* InActor);
	bool ContainsOverlapActor(
		const TArray<FOverlapActorData>& TargetArray,
		const AActor* InActor) const;
	void RemoveOverlapActorFromArray(
		TArray<FOverlapActorData>& TargetArray,
		AActor* InActor);
	float GetCurrentWorldTimeSeconds() const;
	void DebugRemovedOverlapActor(
		const FString& Reason,
		const FOverlapActorData& OverlapData) const;
	void ResetOverlapBlockingActorsForCommand();

	// The physics calculation component that moves the owning pawn by applying velocity.
	UPROPERTY()
	UTrackPhysicsMovement* M_TrackPhysicsMovement;

	// Counter for extra stuck checking.
	float M_TimeAtLowSpeed = 0.f;

	// To switch between teleporting and generating a stuck location to navigate to.
	bool bM_TeleportedLastStuck = false;

	bool bSnapToSpline;

	/* Flag that should be set if you want to force reverse */
	bool bWantsReverse;


	// Used by FindTeleportLocation to get a direction to teleport in. 
	FVector VehicleCurrentDestination;

	/* The average angle ahead within the sample range */
	float AverageAheadAngle = 0.f;


	/* Flag set to true if reversing, used with the threshold */
	bool bReversing = false;

	/* Helper to return the agents current location in the world */
	FVector GetAgentLocation() const;

	/* returns the angle delta between the two vectors, used for figuring out the turn angles */
	float GetVectorAngle(FVector A, FVector B);


	/* Cached pointer for the controlled pawn */
	UPROPERTY()
	APawn* ControlledPawn;

	/* The throttle value thats been calculated by the calculate throttle */
	float CalculatedThrottleValue;


	/* Update Driving Cached Values*/

	float AbsoluteTargetAngle;
	float DestinationDistance;
	float AdjustedDesiredSpeed;

	bool bImplementsInterface;


	/* The last sample index used by the slowdown system, used for debugging */
	int SlowdownSampleIndex = 0;

	/* The largest angle ahead of the vehicle that has been calculated within the slowdown range */
	float SlowdownAngle = 0.f;

	/* The final points distance away */
	float FinalDestinationDistance = 0.f;

	bool bIsLastPathPoint = false;

	/**
     * @brief Decide how to react to one moving allied actor that’s inside/near our forward cone.
     * @param MovementDirection Our own world-space, normalized 2D movement direction.
     * @param OverlapActor The other allied actor we’re evaluating.
     * @param DeltaTime
     * @return EvadeRight when they are entering our cone and headed toward us at a critical AoA;
     *         Wait when they pierce our cone but with a less critical AoA; otherwise MoveNormally.
     */
	EEvasionAction DetermineOverlapWithinMovementDirection(
		const FVector& MovementDirection, AActor* OverlapActor, const float DeltaTime);

	bool ExecuteEvasiveAction(const EEvasionAction Action, const float DeltaTime);
};
