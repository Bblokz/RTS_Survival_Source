// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Procedural/CustomNodes/EnemyBaseSetup/EnemyBaseSetupPlacement.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseTypes.h"

/**
 * @brief One structure of a planned base. ClassIndex indexes the resolver's class table (see
 * FSurvivalSovietLayoutInput::ResolveEntry); Position is the actor pivot in world XY.
 */
struct FSurvivalSovietPlannedStructure
{
	int32 ClassIndex = INDEX_NONE;
	ESurvivalSovietStructureRole Role = ESurvivalSovietStructureRole::Fortification;
	FVector2D Position = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	double UniformScale = 1.0;
};

/** @brief One garrison group of a planned base with its seeded unit picks and spawn spots. */
struct FSurvivalSovietPlannedGarrison
{
	ESurvivalSovietGarrisonBehaviour Behaviour = ESurvivalSovietGarrisonBehaviour::HoldPosition;
	TArray<FTrainingOption> Units;
	TArray<FVector2D> SpawnPositions;
	double FacingYawRadians = 0.0;

	// Patrol only.
	TArray<FVector2D> PatrolPoints;
	int32 IterationsAtPatrolPoint = 2;
	float GuardSecondsPerIteration = 8.f;
	float GuardSphereRadius = 600.f;
};

/** @brief Everything a base needs spawned; produced deterministically from seed, preset and strength. */
struct FSurvivalSovietBasePlan
{
	TArray<FSurvivalSovietPlannedStructure> Structures;
	TArray<FSurvivalSovietPlannedGarrison> Garrison;

	FVector2D Center = FVector2D::ZeroVector;
	// Point on the perimeter facing the player's start location (where base-launched attacks leave).
	FVector2D LaunchPoint = FVector2D::ZeroVector;
	double ThreatYawRadians = 0.0;

	// Largest distance from any production building to its nearest other production building; the strategic AI
	// core cluster distance must be at least this for the base to be detected as one AI base.
	double RequiredCoreClusterDistance = 0.0;
	int32 ProductionCount = 0;
};

/**
 * @brief Inputs of the layout step. ResolveEntry turns a designer entry into a placement entry whose
 * OutputAssetIndex is the class index the plan refers to (and whose footprint is measured or overridden);
 * it returns false when the class cannot be used.
 */
struct FSurvivalSovietLayoutInput
{
	const FSurvivalSovietBasePreset* Preset = nullptr;
	FSurvivalSovietBaseDifficultyScaling Scaling;
	int32 Strength = 0;
	int32 Seed = 0;

	FVector2D Center = FVector2D::ZeroVector;
	double BuildAreaRadius = 6000.0;
	// The player's start location: lines, pits and garrisons face it.
	FVector2D ThreatLocation = FVector2D::ZeroVector;
	TArray<FVector2D> DefensePoints;

	TFunction<bool(const FEnemyBlueprintEntry&, FEnemyResolvedEntry&)> ResolveEntry;
};

/**
 * @brief Pure layout step of a Soviet base: preset + strength + seed -> plan. Drives the shared EnemyBaseSetup
 * placement builder (the engine behind the PCG enemy base nodes) so lines, belts, wire, sandbags and props use the
 * same SAT occupancy as the PCG nodes and never overlap. No UObjects are touched besides the resolver callback,
 * so the same step can later back a PCG node for editor previews.
 */
class FSurvivalSovietBaseLayout
{
public:
	/** @return The plan; empty when the input has no preset or resolver. */
	static FSurvivalSovietBasePlan BuildPlan(const FSurvivalSovietLayoutInput& Input);

	/** @return Copies of a slot on a base of the given strength (0 when the slot is gated out). */
	static int32 GetSlotCount(const FSurvivalSovietStructureSlot& Slot, int32 Strength, float Multiplier = 1.f);
};
