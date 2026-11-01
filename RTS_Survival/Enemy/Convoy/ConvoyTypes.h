// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "ConvoyTypes.generated.h"

class AActor;
class AScavengeableObject;
class ASquadController;
class ATankMaster;

RTS_SURVIVAL_API DECLARE_LOG_CATEGORY_EXTERN(LogConvoy, Log, All);

/** What the trucks carry; decides the cargo cache and which resources it pays. */
UENUM(BlueprintType)
enum class EConvoyCargoType : uint8
{
	Fuel,
	VehicleParts,
	Ammo
};

UENUM(BlueprintType)
enum class EConvoyUnitRole : uint8
{
	Truck,
	EscortVehicle,
	EscortSquad
};

/** Where an escort keeps station around the trucks. */
UENUM(BlueprintType)
enum class EConvoyStation : uint8
{
	// Ahead of the first truck.
	Lead,
	// Beside the trucks, left of the road (spread along the truck column).
	LeftFlank,
	RightFlank,
	// Behind the last truck.
	Rear
};

UENUM(BlueprintType)
enum class EConvoyState : uint8
{
	Spawning,
	// Everything spawned; units drive to their first slots before the column starts.
	Forming,
	Cruise,
	// A truck fell behind its slot; the column ahead of it slows down until it closes up.
	Regroup,
	UnderAttack,
	// Every truck dead; the surviving escorts guard the cargo site or withdraw.
	TrucksLost,
	Finished
};

UENUM(BlueprintType)
enum class EConvoyOutcome : uint8
{
	None,
	// The trucks reached the end of the route; the cargo is gone.
	Escaped,
	// Every truck was destroyed.
	TrucksDestroyed,
	// Stopped from code (EndConvoy) or the world ended.
	Aborted
};

/** What the trucks do while their escort fights. */
UENUM(BlueprintType)
enum class EConvoyTruckCombatBehaviour : uint8
{
	// Drive on at the convoy pace; the escorts that fight catch up afterwards.
	KeepMoving,
	// Stop where they are until the fight is over.
	HaltBehindEscorts
};

/** What the surviving escorts do once every truck is gone. */
UENUM(BlueprintType)
enum class EConvoyEscortsAfterTrucksLost : uint8
{
	// Stand around the cargo caches (fighting anything that comes) for a while, then withdraw.
	GuardCargo,
	// Drive straight on to the end of the route and leave.
	WithdrawToExit
};

UENUM(BlueprintType)
enum class EConvoyMemberState : uint8
{
	Spawning,
	Station,
	Engaging,
	// Leash hit or fight over: driving back to the slot.
	Returning,
	GuardingCargo,
	Withdrawing,
	Dead,
	// Left the convoy alive (exit reached or handed off); restrictions cleared.
	Released
};

UENUM(BlueprintType)
enum class EConvoyEventType : uint8
{
	Started,
	StateChanged,
	UnitSpawned,
	SpawnFailed,
	RegroupStart,
	RegroupEnd,
	UnderAttack,
	EscortEngage,
	EscortReturn,
	TruckDestroyed,
	EscortDestroyed,
	CargoSpawned,
	CargoScavenged,
	CargoExpired,
	Stuck,
	Unstuck,
	Repath,
	SlotReassigned,
	ReachedExit,
	Despawned,
	RestrictionCleared,
	Finished
};

/** One truck class in a convoy definition. */
USTRUCT(BlueprintType)
struct FConvoyTruckSpec
{
	GENERATED_BODY()

	// Spawned as this exact class (the definition is authoritative; no variant mapping).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	TSoftClassPtr<AActor> TruckClass;

	// Names the truck in logs and errors; also used for its training-point value.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	FTrainingOption TrainingOption;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "1"))
	int32 Count = 1;

	// Measured free cruise speed on flat ground (cm/s, the convoy test's T0); the trucks drive at
	// TruckSpeedFraction of it. 0 uses the unit data's top speed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0"))
	float CalibratedCruiseSpeedCms = 0.f;
};

/** One escort entry: a unit type, how many, and where it keeps station. */
USTRUCT(BlueprintType)
struct FConvoyEscortSpec
{
	GENERATED_BODY()

	// Spawned through the async spawner as this training option (enemy director overrides apply).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	FTrainingOption Unit;

	// Optional exact class instead of the training option mapping.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	TSoftClassPtr<AActor> ClassOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	EConvoyStation Station = EConvoyStation::LeftFlank;
};

/** A named guard strength; the caller picks one (survival picks by match round and difficulty). */
USTRUCT(BlueprintType)
struct FConvoyGuardTier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	FName TierName = TEXT("Medium");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	TArray<FConvoyEscortSpec> Escorts;

	// Extra trucks on top of the definition's trucks for this tier (more guards guard more cargo).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0"))
	int32 ExtraTrucks = 0;
};

/** Column geometry, all distances in cm along / across the route. */
USTRUCT(BlueprintType)
struct FConvoySpacingSettings
{
	GENERATED_BODY()

	// Along-route distance between two consecutive trucks.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "300"))
	float TruckGap = 1800.f;

	// How far ahead of the first truck the lead escorts drive.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0"))
	float LeadDistance = 2200.f;

	// How far behind the last truck the rear escorts drive.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0"))
	float RearDistance = 2200.f;

	// Sideways offset of flank vehicles from the route centre line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0"))
	float FlankVehicleOffset = 900.f;

	// Sideways offset of flank squads (they walk outside the vehicles).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0"))
	float FlankSquadOffset = 1400.f;

	// Along-route spacing between escorts that share a lead or rear station.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "200"))
	float StationRowSpacing = 1200.f;

	// Sideways spacing between escorts that share a lead or rear row.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "200"))
	float StationColumnSpacing = 900.f;
};

/** Pace and station keeping. */
USTRUCT(BlueprintType)
struct FConvoySpeedSettings
{
	GENERATED_BODY()

	// Trucks drive at this fraction of their nominal top speed; the convoy pace.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float TruckSpeedFraction = 0.5f;

	// A truck behind its slot may go this much faster than its pace (fraction of its top speed) to close up.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float TruckCatchUpFraction = 0.03f;

	// Speed added per cm of along-route slot error (cm/s per cm); positive when the unit is behind its slot.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0.0"))
	float SlotSpeedGain = 0.35f;

	// Closed loop: how fast the speed trim (restriction / wanted speed) follows the measured ground speed, per second.
	// A restriction of 0.5 does not give half the ground speed (drive train, path following), so it is learned.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0.0"))
	float SpeedTrimGainPerSecond = 0.6f;

	// Escorts may exceed the measured truck speed by at most this factor when catching up.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "1.0"))
	float EscortCatchUpSpeedFactor = 1.6f;

	// Squads march at the convoy pace (raised above their own march speed when needed); this caps the raise.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "100"))
	float MaxSquadMarchSpeedCms = 900.f;

	// Units are sent to a point this far ahead of their slot so they never brake for an arrival.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "500"))
	float LookAheadDistance = 3000.f;

	// A new move order is only given when the wanted target moved this far from the last one.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "100"))
	float ReissueDistance = 1400.f;

	// A truck this far behind its slot starts a regroup (the trucks ahead slow down).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "100"))
	float RegroupLagDistance = 900.f;

	// Seconds the column waits for units to reach their first slots before it starts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0"))
	float FormingSeconds = 6.f;

	// The convoy has escaped when every truck is within this distance of the route end.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "100"))
	float ExitRadius = 1500.f;
};

/** Reaction to attack. */
USTRUCT(BlueprintType)
struct FConvoyCombatSettings
{
	GENERATED_BODY()

	// Hostile units this close to any convoy member put the convoy under attack; escorts this close to the threat engage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "500"))
	float ResponseRadius = 4500.f;

	// An engaging escort never goes further than this from the centre of the trucks.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "500"))
	float LeashDistance = 3500.f;

	// Seconds without a hostile in range (and no member in combat) before the escorts re-form.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "1"))
	float CalmSecondsToResume = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	EConvoyTruckCombatBehaviour TruckBehaviour = EConvoyTruckCombatBehaviour::KeepMoving;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	EConvoyEscortsAfterTrucksLost EscortsAfterTrucksLost = EConvoyEscortsAfterTrucksLost::GuardCargo;

	// GuardCargo: seconds the escorts guard before they withdraw to the exit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0"))
	float GuardCargoSeconds = 90.f;

	// GuardCargo: escorts stand on a ring this far from the cargo centre.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "200"))
	float GuardRingRadius = 1600.f;
};

/** Stuck units, blocked and partial paths. */
USTRUCT(BlueprintType)
struct FConvoyRobustnessSettings
{
	GENERATED_BODY()

	// A unit that should move but made less than this progress in StuckSeconds is stuck.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "10"))
	float StuckProgressCm = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "1"))
	float StuckSeconds = 8.f;

	// Sideways nudge of the re-path target of a stuck unit, growing with each attempt.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0"))
	float StuckNudgeDistance = 700.f;

	// A truck stuck this long no longer holds the column back; it is left behind as a target.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0"))
	float MaxWaitForStuckTruckSeconds = 25.f;
};

/** The cargo a destroyed truck leaves behind. */
USTRUCT(BlueprintType)
struct FConvoyCargoSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	EConvoyCargoType CargoType = EConvoyCargoType::Fuel;

	// The scavengeable cache spawned next to each destroyed truck.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	TSoftClassPtr<AScavengeableObject> CacheClass;

	// Cargo of the whole convoy per training point of its escort (summed and split over the trucks).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy")
	TMap<ERTSResourceType, float> ResourcesPerGuardPoint;

	// Seconds a cache stays before it is gone.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "10"))
	float CacheLifetimeSeconds = 300.f;

	// Seconds it takes a squad to scavenge one cache.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "1"))
	float CacheScavengeSeconds = 12.f;

	// Delay after the truck dies so its own wreck spawns first; the cache lands beside it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0"))
	float CacheSpawnDelaySeconds = 1.5f;

	// Sideways distance of the cache from the truck's death spot.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Convoy", meta = (ClampMin = "0"))
	float CacheSideOffset = 650.f;
};

/** The path a convoy drives; control points in order, start to exit. */
USTRUCT(BlueprintType)
struct FConvoyRoute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convoy")
	FName RouteId = NAME_None;

	// World points the convoy drives through; dense enough that straight lines between them stay drivable
	// (UConvoyRouteLibrary::BuildNavigableRoute produces such a list from a few control points).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convoy")
	TArray<FVector> Points;
};

/** Caller options for one convoy. */
USTRUCT(BlueprintType)
struct FConvoyStartParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convoy")
	int32 OwningPlayer = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convoy")
	int32 GuardTierIndex = 0;

	// Multiplies the whole cargo (difficulty, campaign tuning).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convoy", meta = (ClampMin = "0"))
	float CargoMultiplier = 1.f;

	// Shows a moving mark on the minimap (and marks for the caches).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convoy")
	bool bShowOnMinimap = true;

	// Logged with every event.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convoy")
	FString DebugLabel;
};

/** One entry of the convoy's event stream (logs, tests, game modes). */
USTRUCT(BlueprintType)
struct FConvoyEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	EConvoyEventType Type = EConvoyEventType::Started;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 ConvoyId = INDEX_NONE;

	// Member id inside the convoy; INDEX_NONE for convoy-wide events.
	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	int32 MemberId = INDEX_NONE;

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	FString Detail;

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	double WorldSeconds = 0.0;
};

/** Read-only view of one convoy member for logs and tests. */
USTRUCT(BlueprintType)
struct FConvoyMemberSnapshot
{
	GENERATED_BODY()

	int32 MemberId = INDEX_NONE;
	EConvoyUnitRole Role = EConvoyUnitRole::Truck;
	EConvoyStation Station = EConvoyStation::Lead;
	EConvoyMemberState State = EConvoyMemberState::Spawning;
	FString SlotName;
	TWeakObjectPtr<AActor> Actor;
	FVector Location = FVector::ZeroVector;
	FVector SlotLocation = FVector::ZeroVector;
	float RouteDistance = 0.f;
	float SlotRouteDistance = 0.f;
	float NominalSpeed = 0.f;
	float AppliedSpeedFraction = 1.f;
	float WantedSpeed = 0.f;
	bool bIsStuck = false;
	bool bHasPartialPath = false;
};

/** Value of one cache: what it pays. */
USTRUCT(BlueprintType)
struct FConvoyCargoValue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Convoy")
	TMap<ERTSResourceType, int32> Resources;

	int32 GetTotal() const
	{
		int32 Total = 0;
		for (const TPair<ERTSResourceType, int32>& Pair : Resources)
		{
			Total += Pair.Value;
		}
		return Total;
	}
};

namespace ConvoyNames
{
	RTS_SURVIVAL_API FString GetStateName(EConvoyState State);
	RTS_SURVIVAL_API FString GetEventName(EConvoyEventType Type);
	RTS_SURVIVAL_API FString GetRoleName(EConvoyUnitRole Role);
	RTS_SURVIVAL_API FString GetStationName(EConvoyStation Station);
	RTS_SURVIVAL_API FString GetMemberStateName(EConvoyMemberState State);
	RTS_SURVIVAL_API FString GetOutcomeName(EConvoyOutcome Outcome);
	RTS_SURVIVAL_API FString GetCargoName(EConvoyCargoType CargoType);
}
