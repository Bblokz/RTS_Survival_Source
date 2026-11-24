// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"

#include "ConvoyBehaviourTypes.generated.h"

class AActor;

/** Where a convoy is in its timed stop (FConvoyStopPlan). */
UENUM(BlueprintType)
enum class EConvoyStopPhase : uint8
{
	// This convoy has no stop.
	None,
	// Driving towards the stop point.
	Approaching,
	// The column stands at the stop point.
	Halted,
	// Over: timed out, attacked or cancelled.
	Done
};

UENUM(BlueprintType)
enum class EConvoyStopEndReason : uint8
{
	// The stop lasted its StopSeconds; the column drives on.
	TimedOut,
	// Attacked while standing at the stop: the column floors it.
	Attacked,
	// Attacked before it reached the stop: it never stops and floors it.
	SkippedAttacked,
	// AConvoy::CancelStop, or the column lost all its trucks before the stop ended.
	Cancelled
};

/** What became of one truck; summed into FConvoyCargoOutcome when the convoy finishes. */
UENUM(BlueprintType)
enum class EConvoyTruckFate : uint8
{
	// Still driving, never spawned, or the convoy was ended from code.
	None,
	// Left the map at the route exit.
	Escaped,
	Destroyed,
	// Surrendered to the player (ASurrenderingCargoTruck).
	Captured,
	// Left the column at the fork and reached the branch destination.
	BranchDelivered
};

UENUM(BlueprintType)
enum class EConvoyBehaviourEventType : uint8
{
	StopStarted,
	StopEnded,
	BranchTruckLeftColumn,
	BranchTruckArrived,
	AmbushZoneEntered,
	// The column reached the dismount point (or was attacked first): the route unloads the hunter squads.
	HuntersDismount
};

/** Typed payload of AConvoy::OnBehaviourEvent; only the fields of its Type are filled. */
USTRUCT(BlueprintType)
struct FConvoyBehaviourEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	EConvoyBehaviourEventType Type = EConvoyBehaviourEventType::StopStarted;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 ConvoyId = INDEX_NONE;

	// Branch events: the truck. Only valid during the broadcast (a delivered truck is removed right after it).
	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	TObjectPtr<AActor> Truck = nullptr;

	// Branch events: the truck's cargo.
	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	EConvoyCargoType CargoType = EConvoyCargoType::Fuel;

	// StopEnded only.
	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	EConvoyStopEndReason StopEndReason = EConvoyStopEndReason::TimedOut;

	// AmbushZoneEntered only: index into the zones the convoy was given.
	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 ZoneIndex = INDEX_NONE;

	// AmbushZoneEntered only: the radio post stands, so the convoy speeds up in the zone.
	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	bool bRadioPostAlive = false;

	// HuntersDismount only: where each living truck stands, with its position in the truck column (0 = front).
	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	TArray<FVector> TruckLocations;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	TArray<int32> TruckColumnIndices;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	FVector Location = FVector::ZeroVector;
};

/** Truck fates of one cargo type in a finished convoy. */
USTRUCT(BlueprintType)
struct FConvoyCargoTypeOutcome
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	EConvoyCargoType CargoType = EConvoyCargoType::Fuel;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 EscapedTrucks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 DestroyedTrucks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 CapturedTrucks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 BranchDeliveredTrucks = 0;

	void AddFate(const EConvoyTruckFate Fate);
};

/**
 * What a finished convoy's trucks did, for whoever reacts to escaped cargo. Escaped = drove off at the route exit;
 * branch-delivered trucks reached the branch destination and are not counted as escaped.
 */
USTRUCT(BlueprintType)
struct FConvoyCargoOutcome
{
	GENERATED_BODY()

	// The route's convoy index (0 = the first convoy that route started); INDEX_NONE outside a route.
	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 ConvoyIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 ConvoyId = INDEX_NONE;

	// The definition's cargo type; mixed columns are split in ByCargoType.
	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	EConvoyCargoType CargoType = EConvoyCargoType::Fuel;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	EConvoyOutcome Outcome = EConvoyOutcome::None;

	// Escaped + destroyed + captured + branch-delivered (trucks that never spawned are left out).
	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 TruckCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 EscapedTrucks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 DestroyedTrucks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 CapturedTrucks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 BranchDeliveredTrucks = 0;

	// One entry per cargo type present in the column.
	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	TArray<FConvoyCargoTypeOutcome> ByCargoType;
};

// ---------------------------------------------------------------------------------------------------------------------
// Behaviour plan: resolved data (route distances, world points) the caller hands AConvoy::SetBehaviourPlan.
// ---------------------------------------------------------------------------------------------------------------------

/** A timed halt of the whole column at one point of the route. */
USTRUCT()
struct FConvoyStopPlan
{
	GENERATED_BODY()

	bool bEnabled = false;
	// Distance along the convoy's route where the front truck stops.
	float StopDistance = 0.f;
	FVector StopLocation = FVector::ZeroVector;
	float StopSeconds = 60.f;
	// The stop starts once the front truck is this close (along the route) to StopDistance.
	float ArriveTolerance = 400.f;
	// Attacked before or during the stop: the trucks drive this much faster for FloorItSeconds.
	float FloorItSpeedMultiplier = 1.4f;
	float FloorItSeconds = 30.f;
	// On: flooring it overrides EConvoyTruckCombatBehaviour::HaltBehindEscorts while it lasts.
	bool bFloorItIgnoresHalt = true;
};

/** Trucks of one cargo type leave the column at a fork and drive a second path. */
USTRUCT()
struct FConvoyBranchPlan
{
	GENERATED_BODY()

	bool bEnabled = false;
	EConvoyCargoType CargoType = EConvoyCargoType::Ammo;
	// Distance along the convoy's route where a branch truck turns off.
	float ForkDistance = 0.f;
	// Drivable points from the fork to the branch destination (UConvoyRouteLibrary::BuildNavigableRoute).
	TArray<FVector> BranchPoints;
	float ArrivalRadius = 1500.f;
	// Applied to the trucks' convoy pace while they drive the branch.
	float SpeedMultiplier = 1.f;
	// On: a delivered truck is removed; off: it is released where it stands.
	bool bDespawnOnArrival = true;
	// The column waits this long at the exit for branch trucks still driving; then they count as escaped.
	float MaxWaitAtExitSeconds = 120.f;
};

USTRUCT()
struct FConvoyAmbushZonePlan
{
	GENERATED_BODY()

	FVector Centre = FVector::ZeroVector;
	float Radius = 3000.f;
};

/** Zones the column reports entering; while the radio post stands the trucks speed up inside them. */
USTRUCT()
struct FConvoyAmbushPlan
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FConvoyAmbushZonePlan> Zones;

	// Optional; no speed-up without it.
	UPROPERTY()
	TWeakObjectPtr<AActor> RadioPost = nullptr;

	float SpeedMultiplier = 1.35f;
};

/** Where the trucks unload their infantry; the caller spawns and orders the squads. */
USTRUCT()
struct FConvoyHunterPlan
{
	GENERATED_BODY()

	bool bEnabled = false;
	// Distance along the convoy's route of the dismount point.
	float DismountDistance = 0.f;
	FVector DismountLocation = FVector::ZeroVector;
	// The front truck dismounts this far (along the route) before the dismount point.
	float DismountRadius = 2500.f;
	// On: an attack before the dismount point unloads the squads where the trucks are.
	bool bDismountWhenAttacked = true;
	// The column stands this long while the squads get out.
	float DismountHaltSeconds = 8.f;
};

/** Everything AConvoy::SetBehaviourPlan takes; every part is optional. */
USTRUCT()
struct FConvoyBehaviourPlan
{
	GENERATED_BODY()

	UPROPERTY()
	FConvoyStopPlan Stop;

	UPROPERTY()
	FConvoyBranchPlan Branch;

	UPROPERTY()
	FConvoyAmbushPlan Ambush;

	UPROPERTY()
	FConvoyHunterPlan Hunters;
};

/** Runtime state of the behaviours of one convoy; all timing reads world seconds in the convoy tick (no timers). */
USTRUCT()
struct FConvoyBehaviourRuntime
{
	GENERATED_BODY()

	EConvoyStopPhase StopPhase = EConvoyStopPhase::None;
	double StopStartTime = -1.0;
	// When the front truck came within the brake zone of the stop point (the stop starts at a timeout from there).
	double StopBrakeZoneEnterTime = -1.0;
	double FloorItUntilTime = -1.0;
	// Dismount halt.
	double HaltUntilTime = -1.0;
	bool bHuntersDismounted = false;
	// Per ambush zone: the column centre was inside on the last tick.
	TArray<bool> bInsideZone;
	// The column reached the exit (it may still wait there for branch trucks).
	double ExitReachedTime = -1.0;
};

/** Per truck: driving the branch path. */
USTRUCT()
struct FConvoyMemberBranchState
{
	GENERATED_BODY()

	double LeftColumnTime = -1.0;
	// Projection hint along the branch path.
	float BranchDistance = 0.f;
	bool bHasOrder = false;
	FVector LastOrderTarget = FVector::ZeroVector;
	FVector StuckWindowLocation = FVector::ZeroVector;
	double StuckWindowStart = -1.0;
	int32 StuckAttempts = 0;
};
