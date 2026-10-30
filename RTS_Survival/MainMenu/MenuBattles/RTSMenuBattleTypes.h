// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "RTSMenuBattleTypes.generated.h"

class ARTSMenuBattleArena;

UENUM(BlueprintType)
enum class ERTSMenuBattleSide : uint8
{
	German,
	Soviet
};

UENUM(BlueprintType)
enum class ERTSMenuBattleUnitKind : uint8
{
	Vehicle,
	Squad,
	Aircraft,
	// A weapon pit (building expansion) dug in on its side's line; it fights where it stands.
	Pit
};

// What a battle is mostly fought with; every battle picks one so consecutive battles feel different.
UENUM(BlueprintType)
enum class ERTSMenuBattleFlavour : uint8
{
	CombinedArms,
	Armoured,
	Infantry,
	AirRaid,
	// Both sides hold weapon pits from the start; the waves assault them.
	Entrenched
};

UENUM(BlueprintType)
enum class ERTSMenuBattleShot : uint8
{
	Orbit,
	FlankDolly,
	BehindLine,
	ChaseUnit,
	HighOverview,
	// Alongside a side's moving column, at its pace: the drive-in at the start and reinforcements arriving.
	ColumnTrack
};

/** The unit Blueprints one side of the menu battles picks from; each battle draws a random subset. */
USTRUCT(BlueprintType)
struct FRTSMenuBattleFactionPool
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Units")
	TArray<TSoftClassPtr<AActor>> Vehicles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Units")
	TArray<TSoftClassPtr<AActor>> Squads;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Units")
	TArray<TSoftClassPtr<AActor>> Aircraft;

	// Weapon pits (gun, mortar, flak and MG pits) dug in on the side's line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Units")
	TArray<TSoftClassPtr<AActor>> Pits;

	// Variant Blueprints of this side's vehicles (DA_UnitVariants); drawn instead of a base vehicle at the director's
	// variant chance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variants")
	TArray<TSoftClassPtr<AActor>> VariantVehicles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variants")
	TArray<TSoftClassPtr<AActor>> VariantSquads;
};

/** How many units of each kind one side brings in one wave. */
struct FRTSMenuBattleWaveCounts
{
	int32 Vehicles = 0;
	int32 Squads = 0;
	int32 Aircraft = 0;
	int32 Pits = 0;
};

/** How large a battle an arena holds and how the camera frames it. */
USTRUCT(BlueprintType)
struct FRTSMenuBattleArenaSettings
{
	GENERATED_BODY()

	// Distance between the German and the Soviet line along the arena's arrow; both lines drive into range.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "2000"))
	float LineSeparation = 9000.f;

	// Sideways distance between two units of one rank; wide so units do not block each other on the way in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "400"))
	float SlotSpacing = 4500.f;

	// Distance between the vehicle rank, the infantry rank behind it and the aircraft rank at the back.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "400"))
	float RankSpacing = 1600.f;

	// The opening units start this much further back and drive up to their line before the fighting starts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	float ApproachDistance = 3500.f;

	// Reinforcements arrive this far behind their side's starting line and drive in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	float ReinforcementDepth = 2600.f;

	// A battle opens as a skirmish of a vehicle or two per side...
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escalation", meta = (ClampMin = "1"))
	int32 OpeningVehiclesMax = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escalation", meta = (ClampMin = "0"))
	int32 OpeningSquadsMax = 1;

	// ...and grows with this many reinforcement waves per side.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escalation", meta = (ClampMin = "0"))
	int32 ReinforcementWavesMin = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escalation", meta = (ClampMin = "0"))
	int32 ReinforcementWavesMax = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escalation", meta = (ClampMin = "3"))
	float WaveIntervalSeconds = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escalation", meta = (ClampMin = "1"))
	int32 WaveVehiclesMax = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escalation", meta = (ClampMin = "0"))
	int32 WaveSquadsMax = 2;

	// Chance per side and reinforcement wave that an aircraft joins.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escalation", meta = (ClampMin = "0", ClampMax = "1"))
	float AircraftChancePerWave = 0.3f;

	// Chance per side that a weapon pit is dug in on its line when the battle opens (Entrenched battles always have 2+).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escalation", meta = (ClampMin = "0", ClampMax = "1"))
	float PitChancePerSide = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escalation", meta = (ClampMin = "1"))
	int32 MaxPitsPerSide = 3;

	// Vehicles and squads one side brings over the whole battle at most.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escalation", meta = (ClampMin = "1"))
	int32 MaxGroundUnitsPerSide = 14;

	// Battles here leave the fog of war, as on landscapes outside the fog manager's map extent.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	bool bKeepUnitsOutOfFog = false;

	// Base distance of the camera to the fighting; every shot scales it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "1000"))
	float CameraDistance = 4500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "200"))
	float CameraHeight = 2300.f;

	// Landscape the arena stands on; the next battle always moves to another region when there is one.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection")
	FName Region = TEXT("Desert");

	// Relative chance this arena hosts the next battle.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0"))
	float PickWeight = 1.f;
};

/** One unit picked for the next battle, with its place in its side's ranks. */
USTRUCT()
struct FRTSMenuBattleRosterEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TSoftClassPtr<AActor> UnitClass;

	ERTSMenuBattleSide Side = ERTSMenuBattleSide::German;
	ERTSMenuBattleUnitKind Kind = ERTSMenuBattleUnitKind::Vehicle;
	// 0 is the opening skirmish; later waves are reinforcements.
	int32 Wave = 0;
	// Starts ApproachDistance behind its slot and drives up to it before the battle's attack orders begin.
	bool bDrivesIn = false;
	int32 SlotIndex = 0;
	// Random offset from the slot (sideways, along the battle axis) so the lines do not stand on a grid.
	FVector2D SlotJitter = FVector2D::ZeroVector;
};

/** The arena and units of a battle that is loading or about to start. */
USTRUCT()
struct FRTSMenuBattleRoster
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ARTSMenuBattleArena> Arena;

	UPROPERTY()
	TArray<FRTSMenuBattleRosterEntry> Entries;

	// Seed of the camera's shot choices for this battle.
	int32 CameraSeed = 0;

	ERTSMenuBattleFlavour Flavour = ERTSMenuBattleFlavour::CombinedArms;
	int32 WaveCount = 0;

	bool GetIsValid() const { return Arena.IsValid() && Entries.Num() > 0; }
};

/** Units of one side of the running battle. */
USTRUCT()
struct FRTSMenuBattleSideUnits
{
	GENERATED_BODY()

	// Vehicles and soldiers: the side has lost when none of them is alive.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> GroundUnits;

	// Soldiers are commanded through their squad controller.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Squads;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Aircraft;
};

/** State of the battle the camera is filming. */
USTRUCT()
struct FRTSMenuBattleState
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ARTSMenuBattleArena> Arena;

	UPROPERTY()
	FRTSMenuBattleSideUnits German;

	UPROPERTY()
	FRTSMenuBattleSideUnits Soviet;

	// Wrecks and team weapons the battle spawned; they leave with the battle.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SpawnedByBattle;

	// Target each attacker was last sent at; a new order only follows once that target is dead.
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<AActor>> OrderedTargets;

	// How often this battle had to show a hidden unit again; above zero something still hides units that left the fog.
	int32 HiddenUnitsShownAgain = 0;

	// Attackers driving toward a target the fog of war still hides, with the time of their last move order.
	TMap<TWeakObjectPtr<AActor>, float> AdvanceOrderSeconds;

	// When and where each attacker got its last attack order: orders are renewed now and then, and an attacker that
	// has not moved since (an aborted path) is driven most of the way to its target instead.
	TMap<TWeakObjectPtr<AActor>, float> AttackOrderSeconds;
	TMap<TWeakObjectPtr<AActor>, FVector> AttackOrderLocations;

	// Reinforcements still to arrive, sorted by wave.
	UPROPERTY()
	TArray<FRTSMenuBattleRosterEntry> PendingEntries;

	int32 NextWave = 1;
	int32 WaveCount = 0;
	float NextWaveTimeSeconds = 0.f;
	float LastWaveTimeSeconds = 0.f;
	// Attack orders wait until the opening units have driven up to their lines.
	float ApproachEndSeconds = 0.f;
	bool bKeepUnitsOutOfFog = false;

	FDelegateHandle ActorSpawnedHandle;
	float BeginTimeSeconds = -1.f;
	float DecidedTimeSeconds = -1.f;
	int32 BattleNumber = 0;

	bool GetIsActive() const { return Arena.IsValid() && BeginTimeSeconds >= 0.f; }
	bool GetIsDecided() const { return DecidedTimeSeconds >= 0.f; }
	bool GetHasPendingWaves() const { return PendingEntries.Num() > 0; }
};
