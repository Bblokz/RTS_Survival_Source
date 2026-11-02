// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Procedural/CustomNodes/EnemyBaseSetup/EnemyBaseSetupPlacement.h"
#include "RTS_Survival/SurvivalMode/SovietBases/PCG/SurvivalSovietBasePCGNodes.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseSite.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseTypes.h"

/** @brief How a building's ground is dressed beyond its pad decal (round 4: foundations). */
enum class ESurvivalSovietFoundationStyle : uint8
{
	None,
	// Factories, forges, cooling towers, research: a weathered concrete slab the building stands on, a wide concrete apron
	// to the road, scrap and drums in a work corner.
	Concrete,
	// Tents: a trampled-earth oval, large planks to the road, a crate and water can at the door.
	Timber,
	// Barracks (Quonset): a gravel walk round the berm (the berm stays on sand), a duckboard walk to the road, firewood.
	Gravel,
	// Vehicle, ammunition and fuel shelters: oily gravel hardstanding and a rutted track to the road, jerrycans and wheels.
	Hardstanding,
	// The command bunker: a sandbagged concrete forecourt on the door side only (the mesh has its own plinth).
	Forecourt
};

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
	// On top of UniformScale: wall pieces are stretched along the wall to fit their span. A pad keeps its half size here.
	FVector2D StretchXY = FVector2D(1.0, 1.0);
	// Threat points paid for this structure; destroying it counts towards the base's defences.
	int32 ThreatPoints = 0;
	ESurvivalSovietPadKind PadKind = ESurvivalSovietPadKind::None;
	// Pads only: how the building's ground is dressed, its footprint, and the way to the nearest road (door side).
	ESurvivalSovietFoundationStyle FoundationStyle = ESurvivalSovietFoundationStyle::None;
	FVector2D FootprintHalf = FVector2D::ZeroVector;
	FVector2D DoorDirection = FVector2D::ZeroVector;
	double ApronLength = 0.0;
};

/** @brief What the wall layout really built; the manifest prints it and the matrix checks it. */
struct FSurvivalSovietWallReport
{
	ESurvivalSovietWallFamily Family = ESurvivalSovietWallFamily::None;
	int32 Pieces = 0;
	int32 FlagPieces = 0;
	int32 Gates = 0;
	int32 Corners = 0;
	// Wall slots that bad ground, a road or a building left open.
	int32 BlockedSlots = 0;
	double MaxStretch = 1.0;
};

/** @brief What a wall piece is in its family; the manifest's piece list names it. */
enum class ESurvivalSovietWallPieceKind : uint8
{
	Straight,
	End,
	Gate,
	Corner
};

/** @brief One wall piece as laid: its joints on the wall line, for the manifest's joint and flag checks. */
struct FSurvivalSovietPlannedWallPiece
{
	FString ClassName;
	ESurvivalSovietWallPieceKind Kind = ESurvivalSovietWallPieceKind::Straight;
	// Actor pivot in world XY and yaw: how the manifest finds the spawned actor to measure its visible mesh.
	FVector2D Pivot = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	double NominalLength = 0.0;
	// Span the piece belongs to; pieces of one span touch joint to joint.
	int32 Span = INDEX_NONE;
	FVector2D JointStart = FVector2D::ZeroVector;
	FVector2D JointEnd = FVector2D::ZeroVector;
	double Stretch = 1.0;
	bool bFlag = false;
	bool bGate = false;
};

/** @brief A building's ground pad as planned, for the manifest. */
struct FSurvivalSovietPlannedPad
{
	// Resolver class index of the building, as in FSurvivalSovietPlannedStructure::ClassIndex.
	int32 BuildingClassIndex = INDEX_NONE;
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D PadHalfExtents = FVector2D::ZeroVector;
	FVector2D FootprintHalfExtents = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	ESurvivalSovietPadKind Kind = ESurvivalSovietPadKind::None;
};

/** @brief What a storage yard holds; fuel and ammunition never share one. */
enum class ESurvivalSovietYardKind : uint8
{
	Ammo,
	Fuel
};

/** @brief A fenced storage yard as built, for the manifest's yard checks. */
struct FSurvivalSovietPlannedYard
{
	ESurvivalSovietYardKind Kind = ESurvivalSovietYardKind::Ammo;
	FVector2D Center = FVector2D::ZeroVector;
	// Along the yard's street side, then towards the street.
	FVector2D HalfExtents = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	TMap<FString, int32> ItemCounts;
	// World XY of every stack, for the fuel to ammunition distance.
	TArray<FVector2D> ItemPositions;
	int32 Rows = 0;
	double AisleWidth = 0.0;
	double StackArea = 0.0;
	FString FenceClass;
	int32 FencePieces = 0;
	// Fence slots left open by a street, a building or bad ground, beside the entrance.
	int32 FenceGaps = 0;
	// What held each open fence slot: occupancy category names, or "ground" for the terrain and yard predicate.
	TMap<FString, int32> FenceGapCauses;
	// Where each open fence slot or corner is (world XY), for the open-slot check.
	TArray<FVector2D> FenceGapPositions;
	// How many times a side was pulled in so its whole fence line is free.
	int32 FenceTightenSteps = 0;
	// The stack list the rows were filled from ("own" = the plot's dressing, "other" = the other ammunition list), the
	// number of stack types kept and the cell count the yard was planned for.
	FString StackList;
	int32 StackTypes = 0;
	int32 StackEstimate = 0;
	FVector2D Entrance = FVector2D::ZeroVector;
	TMap<FString, int32> NetCounts;
	double NetArea = 0.0;
	// The preset's ammunition stores or fuel tanks standing in this yard.
	TArray<FString> StoreClasses;
	// Class indices of any other building inside the yard: must stay empty.
	TArray<int32> BuildingsInside;
};

/** @brief A lamp post and what it lights: a gate, a junction or a yard entrance. */
struct FSurvivalSovietPlannedLantern
{
	FVector2D Position = FVector2D::ZeroVector;
	FString Lights;
	FVector2D Target = FVector2D::ZeroVector;
};

/** @brief A gate as built: where its middle stands on the wall line and which way is out. */
struct FSurvivalSovietPlannedGate
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D Outward = FVector2D(1.0, 0.0);
	double Width = 0.0;
};

/** @brief Which surface of the site kit a street uses. */
enum class ESurvivalSovietRoadKind : uint8
{
	// Reserved ground without a surface, such as a parade square.
	None,
	Main,
	Lane,
	Strip,
	// Narrow two-track to an outlying post: the third width of the street hierarchy.
	Track
};

/** @brief One street of a planned base; spawned as a ground spline through Points (world XY). */
struct FSurvivalSovietPlannedRoad
{
	ESurvivalSovietRoadKind Kind = ESurvivalSovietRoadKind::Main;
	TArray<FVector2D> Points;
	double Width = 0.0;
};

/** @brief One garrison group of a planned base with its seeded unit picks and posts. */
struct FSurvivalSovietPlannedGarrison
{
	ESurvivalSovietStepKind Kind = ESurvivalSovietStepKind::HoldGarrison;
	TArray<FTrainingOption> Units;
	// Parallel to Units.
	TArray<FVector2D> SpawnPositions;
	TArray<double> SpawnYaws;

	// Patrol only.
	TArray<FVector2D> PatrolPoints;
};

/** @brief Everything a base needs spawned; produced deterministically from seed, preset and strength. */
struct FSurvivalSovietBasePlan
{
	TArray<FSurvivalSovietPlannedStructure> Structures;
	TArray<FSurvivalSovietPlannedRoad> Roads;
	TArray<FSurvivalSovietPlannedGarrison> Garrison;

	FVector2D Center = FVector2D::ZeroVector;
	// Outside the main gate, on the street that leaves the base (where base-launched attacks leave).
	FVector2D LaunchPoint = FVector2D::ZeroVector;
	double ThreatYawRadians = 0.0;
	double AxisYawRadians = 0.0;

	// Smallest distance at which all production buildings chain into one cluster; the strategic AI core cluster
	// distance must be at least this for the base to be detected as one AI base.
	double RequiredCoreClusterDistance = 0.0;
	int32 ProductionCount = 0;

	// What the base really got for its budget: structures that found a spot plus garrison.
	int32 ThreatPoints = 0;
	int32 ReservePoints = 0;
	int32 ThreatBudget = 0;
	int32 UnspentPoints = 0;

	// Declared by the preset for this strength and tech cap but not placed: "class (reason)".
	TArray<FString> DroppedSlots;
	// Emplacement spots the plan offered and took, and threat points standing away from the main line.
	int32 EmplacementSpotsAvailable = 0;
	int32 EmplacementSpotsUsed = 0;
	int32 PointsOutsideMainLine = 0;

	FSurvivalSovietWallReport Wall;
	// Every wall piece in the order it was laid, for the joint checks of the preview manifest.
	TArray<FSurvivalSovietPlannedWallPiece> WallPieces;
	TArray<FSurvivalSovietPlannedGate> Gates;
	TArray<FSurvivalSovietPlannedPad> Pads;
	TArray<FSurvivalSovietPlannedYard> Yards;
	TArray<FSurvivalSovietPlannedLantern> Lanterns;
	// Wall slots left open (centre, outward, slot length), each pictured by the preview.
	TArray<FSurvivalSovietPlannedGate> OpenWallSlots;
	// World XY; bunkers must stand beyond this closed line.
	TArray<FVector2D> PerimeterLine;
	// Broken placement rules, "rule: what". Must stay empty; the manifest and the matrix fail on any entry.
	TArray<FString> RuleViolations;
	int32 BuildingsTotal = 0;
	int32 BuildingsOnPad = 0;
	// Buildings too small for a pad that have no footprint design (poles, megaphones): not counted against the pad ratio.
	TArray<FString> PadExemptClasses;
};

/**
 * @brief Inputs of the layout step. ResolveEntry turns a designer entry into a placement entry whose
 * OutputAssetIndex is the class index the plan refers to (and whose footprint is measured or overridden);
 * it returns false when the class cannot be used.
 */
struct FSurvivalSovietLayoutInput
{
	const FSurvivalSovietBasePreset* Preset = nullptr;
	const FSurvivalSovietSiteKit* Kit = nullptr;
	// Optional: without it all ground counts as level and open.
	const FSurvivalSovietSiteMap* Site = nullptr;
	FSurvivalSovietBaseDifficultyScaling Scaling;
	int32 Strength = 0;
	int32 Seed = 0;
	// From the difficulty row, by the base's distance rank.
	int32 ThreatBudget = 0;
	int32 ReserveBudget = 0;

	FVector2D Center = FVector2D::ZeroVector;
	double BuildAreaRadius = 6000.0;
	// The player's start location: the threatened side of every base.
	FVector2D ThreatLocation = FVector2D::ZeroVector;
	TArray<FVector2D> DefensePoints;

	TFunction<bool(const FEnemyBlueprintEntry&, FEnemyResolvedEntry&)> ResolveEntry;
};

/**
 * @brief One base layout in progress: the PCG graph runs it stage by stage, with its occupancy kept between the nodes.
 * @note Keeps a copy of the input; pointers inside the input must outlive the run.
 */
class FSurvivalSovietBaseLayoutRun
{
public:
	explicit FSurvivalSovietBaseLayoutRun(const FSurvivalSovietLayoutInput& Input);
	~FSurvivalSovietBaseLayoutRun();

	/** @brief Runs one stage; what it places is appended to InOutPlan. */
	void RunStage(ESurvivalSovietPlanStage Stage, FSurvivalSovietBasePlan& InOutPlan);

private:
	struct FImpl;
	TUniquePtr<FImpl> M_Impl;
};

/**
 * @brief Pure layout of a Soviet base: preset + strength + seed -> plan. An archetype's master plan (streets, plots,
 * perimeter, emplacements) is executed through the shared EnemyBaseSetup placement builder, the engine behind the PCG
 * enemy base nodes. Streets are reserved first, which is what keeps the base drivable. No UObjects are touched besides
 * the resolver callback. The PCG graph runs the same stages through nodes; BuildPlan runs them in one go.
 */
class FSurvivalSovietBaseLayout
{
public:
	/** @return The plan; empty when the input has no preset, kit or resolver. */
	static FSurvivalSovietBasePlan BuildPlan(const FSurvivalSovietLayoutInput& Input);

	/** @return Copies of a slot on a base of this strength and difficulty (0 when the slot is gated out). */
	static int32 GetSlotCount(const FSurvivalSovietStructureSlot& Slot, int32 Strength, ERTSGameDifficulty Difficulty);

	/** @return What a unit costs the threat budget: its strategic AI training price. */
	static int32 GetUnitThreatPoints(const FTrainingOption& Option);
};
