// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTSVehicleAvoidanceSubsystem.h"
#include "RTSVehicleFootprint.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

class AActor;
class APawn;
class UTrackPathFollowingComponent;

/** Why the last move of a vehicle ended where it did. */
enum class EVehicleMoveEnd : uint8
{
	None,
	// Vehicles sent to the same spot already stood there.
	SettledAroundSharedGoal,
	// A vehicle that stays stood on the goal.
	SettledBesideCoveredGoal,
	// Vehicles that stay kept it from the last stretch.
	SettledNearGoal,
	// Drove to the end of its path.
	Arrived,
	// The engine gave the move up: no progress for too long.
	Blocked,
	// Replaced or cancelled before it was done.
	Aborted
};

enum class EVehicleAvoidanceMode : uint8
{
	// Steers around what is ahead and waits behind what it cannot pass yet.
	Driving,
	// Waited too long: creeps on, squeezes past and nudges what stands in the way.
	Flocking
};

/** What a driving vehicle remembers between frames about the vehicles around it. */
struct FVehicleAvoidanceState
{
	TArray<FRTSVehicleAvoidanceAgent> Neighbours;
	FRTSVehicleFootprint Footprint;
	float SecondsUntilNeighbourRefresh = 0.f;

	EVehicleAvoidanceMode Mode = EVehicleAvoidanceMode::Driving;
	// Time spent standing still behind other vehicles; decides when waiting turns into flocking.
	float WaitingSeconds = 0.f;
	float FlockingSecondsLeft = 0.f;
	// Standing still without getting anywhere; after long enough the engine may call the move blocked again.
	float BlockedSeconds = 0.f;
	FVector BlockedAnchorLocation = FVector::ZeroVector;
	// Time the goal could not be reached because vehicles that will not leave stand on or around it.
	float GoalBlockedSeconds = 0.f;
	float SecondsUntilDisplacementAllowed = 0.f;

	// A path made before a vehicle parked on it leads straight into that vehicle; a new one goes around it.
	float SecondsUntilRepathAllowed = 0.f;
	bool bWantsRepath = false;
	bool bIsRepathing = false;

	// Leaning on a hull without moving; after a moment the vehicle backs off to find another way past.
	float WedgedSeconds = 0.f;
	float BackOffSecondsLeft = 0.f;
	float SecondsUntilBackOffAllowed = 0.f;

	// The side chosen to pass the nearest vehicle ahead is kept, so the vehicle does not weave left and right.
	TWeakObjectPtr<const APawn> PassSideVehicle;
	float PassSide = 1.f;
	// In a knot of vehicles the nearest one keeps changing; the side stays chosen for a moment regardless of which.
	float PassSideHoldSecondsLeft = 0.f;

	// Where the last move order wanted this vehicle; vehicles sent to one spot recognise each other by it.
	FVector LastGoalLocation = FVector::ZeroVector;
	bool bHasLastGoalLocation = false;
	bool bShouldSettleNearGoal = false;
	bool bIsRegistered = false;

	EVehicleMoveEnd LastMoveEnd = EVehicleMoveEnd::None;

	// What was decided last frame: who is being waited for, and the speed cap that tells others this vehicle
	// waits on purpose.
	TWeakObjectPtr<const APawn> LastBlocker;
	float LastSpeedCap = MAX_flt;

#if RTS_WITH_MAP_TESTS
	// Only kept for the map tests to report; none of this exists in a shipping build.
	// How far the end of the last path lay from where the order wanted the vehicle; large for a path cut short.
	float LastPathEndToGoalDistance = 0.f;
	// How often this vehicle told an idle ally to make room.
	int32 DisplacementsAsked = 0;
	float LastBlockerGap = 0.f;
	float LastPush = 0.f;
#endif

	void ResetForNewPath()
	{
		LastMoveEnd = EVehicleMoveEnd::None;
		LastSpeedCap = MAX_flt;
		Mode = EVehicleAvoidanceMode::Driving;
		WaitingSeconds = 0.f;
		FlockingSecondsLeft = 0.f;
		BlockedSeconds = 0.f;
		GoalBlockedSeconds = 0.f;
		WedgedSeconds = 0.f;
		BackOffSecondsLeft = 0.f;
		bShouldSettleNearGoal = false;
		PassSideVehicle.Reset();
	}
};

/** What the avoidance wants from the driving code this frame. */
struct FVehicleAvoidanceOutput
{
	// Where to steer to; the path point itself when nothing is in the way.
	FVector SteeringDestination = FVector::ZeroVector;
	float SpeedCap = MAX_flt;
	float ThrottleLimit = 1.f;
	// Part of its drive force the vehicle may use; below 1 while it waits or leans on another, so shoves stay gentle.
	float DriveStrength = 1.f;
	// Wedged against another hull: drive backwards for a moment with this steering instead of following the path.
	bool bShouldBackOff = false;
	float BackOffSteering = 0.f;
	bool bIsLimitingSpeed = false;
	bool bShouldSuppressBlockDetection = false;
};

/** Everything about the driving vehicle that every neighbour is weighed against, plus what they add up to. */
struct FVehicleAvoidanceFrame
{
	FVector2D Location = FVector2D::ZeroVector;
	FVector2D Forward = FVector2D::ZeroVector;
	// Towards the path point: where the vehicle wants to go.
	FVector2D TravelDirection = FVector2D::ZeroVector;
	FVector2D TravelRight = FVector2D::ZeroVector;
	// Where the hull goes when throttle is given: forward, or backward while reversing.
	FVector2D DriveDirection = FVector2D::ZeroVector;
	FVector2D DriveRight = FVector2D::ZeroVector;
	FVector2D GoalLocation = FVector2D::ZeroVector;
	float Speed = 0.f;
	float LookAhead = 0.f;
	float DistanceToGoal = 0.f;

	float LateralPush = 0.f;
	float SpeedCap = MAX_flt;
	float NearestPassingGap = MAX_flt;

	EVehicleMoveEnd PendingMoveEnd = EVehicleMoveEnd::None;

	// The vehicle in the driving corridor that is closest: the one being waited for.
	const APawn* Blocker = nullptr;
	const UTrackPathFollowingComponent* BlockerPathFollowing = nullptr;
	float BlockerGap = MAX_flt;
	bool bIsBlockerStationary = false;
	bool bIsBlockerStraightAhead = false;
	bool bIsGoalCovered = false;
};
