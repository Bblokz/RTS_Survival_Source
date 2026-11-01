// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Procedural/CustomNodes/EnemyBaseSetup/PCGEnemyBaseSetupTypes.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "SurvivalSovietBaseTypes.generated.h"

/**
 * @brief What a spawned Soviet base structure is for; decides bookkeeping after spawning.
 */
UENUM(BlueprintType)
enum class ESurvivalSovietStructureRole : uint8
{
	// Barracks / factory / research centre: unlocks enemy tech, anchors the strategic AI base cluster and the
	// base counts as destroyed once all of them are dead.
	Production,
	// Storage, towers and other buildings inside the base.
	Support,
	// Bunker of a perimeter line.
	PerimeterBunker,
	// Gun pit on an emplacement, a ridge or a ramp head.
	GunPit,
	// Sandbags, hedgehogs, barbed wire, walls and props.
	Fortification,
	// The ground pad under a building: no actor to fight, no collision.
	Foundation
};

/** @brief The sets of wall pieces that belong together; see FSurvivalSovietWallFamily. */
UENUM(BlueprintType)
enum class ESurvivalSovietWallFamily : uint8
{
	// No wall at all.
	None,
	// Heavy Soviet concrete: straight, flag, rounded end pieces and the arch gate.
	SovietConcrete,
	// Thin precast fence with its corner and swing gate.
	PostApoConcrete,
	Sandbag,
	WireFence,
	// Chain-link panels on posts: the fence of fuel yards.
	ChainLink
};

/** @brief The surface of the ground pad under a building. */
UENUM(BlueprintType)
enum class ESurvivalSovietPadKind : uint8
{
	None,
	Concrete,
	Gravel,
	PackedEarth
};

/**
 * @brief The master plan a preset is built on. Each one is a different street plan, perimeter and zoning, not a
 * reshuffle of one template; see SurvivalSovietBaseArchetypes.cpp.
 */
UENUM(BlueprintType)
enum class ESurvivalSovietBaseArchetype : uint8
{
	// Wagon wheel: ring road, guns on the rim facing out, command and ammunition at the hub, one rear gate.
	Firebase,
	// Parade axis with rows of barracks along parallel lanes inside a light fence.
	Cantonment,
	// Parallel lanes of tank revetments off a boulevard, workshop at the end, fuel point kept off to a flank.
	TankPark,
	// Walled square split into blocks by a cross of fire breaks; fuel gets its own block away from ammunition.
	FortifiedDepot,
	// Chamfered walled fortress: avenues from two gates meet at a central keep, one function per quadrant.
	CommandFortress,
	// No wall: strongpoints on ridges and ramp heads around a tight core, trench arcs across the open approaches.
	HillStronghold,
	// A long strip with hangars along one side and the camp behind them.
	Airstrip
};

/**
 * @brief The purpose of a building or gun; a master plan offers plots and emplacements per zone, which is what keeps
 * fuel away from ammunition and artillery behind the line.
 */
UENUM(BlueprintType)
enum class ESurvivalSovietZone : uint8
{
	Command,
	Barracks,
	Workshop,
	Depot,
	Fuel,
	// Guard towers and loudspeakers.
	Watch,
	// Indirect fire, kept well behind the line.
	Artillery,
	// Direct fire guns, on the threatened side.
	AntiTank,
	AirDefense
};

/**
 * @brief How a garrison group behaves once it has spawned.
 */
UENUM(BlueprintType)
enum class ESurvivalSovietGarrisonBehaviour : uint8
{
	// Stays at a post (vehicles in the revetments, infantry on the parapets and gates) and engages what comes in range.
	HoldPosition,
	// Patrols the base's streets with attack-move support logic (enemy formation controller).
	Patrol,
	// Registered as idle direct-control unit on the strategic AI blackboard; the AI may pull it into its own
	// actions (counter attacks, flanks, retreats).
	StrategicAIReserve
};

/**
 * @brief Where the per-match generation seed comes from.
 */
UENUM(BlueprintType)
enum class ESurvivalSovietSeedMode : uint8
{
	// A new seed every match: bases differ every run.
	RandomPerMatch,
	// Always FixedSeed: identical bases every run (testing, daily challenge).
	Fixed
};

/**
 * @brief One building the base places (production, support or gun pit), gated by base strength.
 * @note Building is the shared EnemyBaseSetup entry: class, weight, optional footprint override, scale range.
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietStructureSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure")
	FEnemyBlueprintEntry Building;

	// Which part of the master plan this structure goes to.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure")
	ESurvivalSovietZone Zone = ESurvivalSovietZone::Depot;

	// The slot is skipped on bases weaker than this (0 = weakest).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "0"))
	int32 MinStrength = 0;

	// The slot is skipped on difficulties below this one: base content that only harder games field.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure")
	ERTSGameDifficulty MinDifficulty = ERTSGameDifficulty::NewToRTS;

	// Production tier this building unlocks for the enemy AI (0 = barracks, 1 = light tank factory,
	// 2 = tier 2 research centre); -1 = not a production building. Difficulty caps the highest tier spawned.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "-1"))
	int32 ProductionTier = -1;

	// Copies placed on a strength 0 base.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "0"))
	int32 Count = 1;

	// Extra copies per strength level above MinStrength (rounded).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "0.0"))
	float CountPerStrength = 0.f;

	// Extra copies per difficulty step above MinDifficulty (rounded).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "0.0"))
	float CountPerDifficulty = 0.f;
};

/** @brief What a step of a preset's threat path buys. */
UENUM(BlueprintType)
enum class ESurvivalSovietStepKind : uint8
{
	// A gun pit or tower on one of the master plan's emplacements of Zone; ridges and ramp heads come first.
	Emplacement,
	// A bunker set into the threatened runs of the perimeter.
	LineBunker,
	// Units that hold a post: vehicles in the revetments, infantry on the parapets and gates.
	HoldGarrison,
	// Units that patrol the base's streets.
	PatrolGarrison,
	// Units the strategic AI may pull into its own actions; capped by the difficulty's reserve points.
	Reserve
};

/**
 * @brief One line of a preset's threat path: the archetype's own scaling order. A base buys down the path in passes
 * until its threat budget is spent, so a bigger budget means more of this list, in this order, and never a rounded
 * multiplier. The gates decide which steps a difficulty or a strength may buy at all.
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietThreatStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Step")
	ESurvivalSovietStepKind Kind = ESurvivalSovietStepKind::Emplacement;

	// Emplacement only: which emplacements of the master plan it may take.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Step",
		meta = (EditCondition = "Kind == ESurvivalSovietStepKind::Emplacement"))
	ESurvivalSovietZone Zone = ESurvivalSovietZone::AntiTank;

	// Emplacement and LineBunker: what is built.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Step")
	FEnemyBlueprintEntry Building;

	// Emplacement and LineBunker: threat points paid per copy (see the price table in 50_SovietBases.md).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Step", meta = (ClampMin = "1"))
	int32 ThreatPoints = 8;

	// Garrison kinds: one option is picked (seeded) per unit among those the difficulty's unit price ceiling allows.
	// Units pay their strategic AI training price.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Step")
	TArray<FTrainingOption> UnitOptions;

	// Copies this step may buy on one base.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Step", meta = (ClampMin = "1"))
	int32 MaxCopies = 4;

	// First pass down the path that may buy this step: later entries of the scaling order.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gates", meta = (ClampMin = "0"))
	int32 FirstPass = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gates", meta = (ClampMin = "0"))
	int32 MinStrength = 0;

	// The first difficulty that fields this step: how a difficulty gets its named capability.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gates")
	ERTSGameDifficulty MinDifficulty = ERTSGameDifficulty::NewToRTS;

	// Skipped when the difficulty's MaxProductionTier is below this: no tanks without a tank factory.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gates", meta = (ClampMin = "0"))
	int32 RequiredProductionTier = 0;

	// This archetype's mark of the step's MinDifficulty: on exactly that difficulty, every base from
	// SignatureMinStrength buys one copy before anything else, so the tier shows on every base and not on some.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gates")
	bool bSignature = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gates", meta = (ClampMin = "0", EditCondition = "bSignature"))
	int32 SignatureMinStrength = 1;
};

/**
 * @brief The shared builders' yard every master plan draws from: fences, gates, revetments, props and road surfaces.
 * Presets choose the buildings and guns; these pieces make the base read as an installation.
 * @note Set SandbagFrontAxis on segment entries: the axis across the run. It is what lays a long piece along its run.
 */
/**
 * @brief One wall piece with where its ends are. A wall is walked with Outward on the right hand (Along = Outward
 * turned +90 degrees), the piece runs from its start joint Length cm along the walk.
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietWallPiece
{
	GENERATED_BODY()

	// SandbagFrontAxis names the local axis that faces out of the base.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	FEnemyBlueprintEntry Entry;

	// Distance between the piece's two joints, cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall", meta = (ClampMin = "0.0"))
	float Length = 0.f;

	// From the start joint to the actor pivot, along the walk, cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	float PivotAlong = 0.f;

	// From the wall's centre line to the actor pivot, towards the outside, cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	float PivotAcross = 0.f;

	bool GetIsSet() const { return Length > 0.f and not Entry.BlueprintClass.IsNull(); }
};

/**
 * @brief Pieces that belong together. One base uses one family for its whole perimeter, the wall layout fits each span
 * between corners and gates exactly and stretches the pieces by the little that is left.
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietWallFamily
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	ESurvivalSovietWallFamily Family = ESurvivalSovietWallFamily::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	FSurvivalSovietWallPiece Straight;

	// Same size as Straight, with a flag on it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	FSurvivalSovietWallPiece FlagStraight;

	// First and last piece of a span at a corner or a free end; rounded ends meet well at any angle.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	FSurvivalSovietWallPiece StartCap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	FSurvivalSovietWallPiece EndCap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	FSurvivalSovietWallPiece FlagStartCap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	FSurvivalSovietWallPiece FlagEndCap;

	// Chance that a piece is its flag variant; two flags never stand side by side.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlagRate = 0.f;

	// Plain pieces that must stand between two flags along the whole loop, corners included.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall", meta = (ClampMin = "1"))
	int32 FlagMinGap = 3;

	// Right-angle corner: pivot on the corner point, one arm along local +X, the other along local -Y. Length = arm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	FSurvivalSovietWallPiece Corner;

	// Spans the road on its own; without it a gate is an opening of GateOpening with GateLeaf and barrier blocks.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	FSurvivalSovietWallPiece Gate;

	// Hinged panel stood open at both sides of an opening; pivot on the hinge, the panel along local +Y.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall")
	FSurvivalSovietWallPiece GateLeaf;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall", meta = (ClampMin = "0.0"))
	float GateOpening = 1300.f;

	// Across the wall, cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall", meta = (ClampMin = "0.0"))
	float Thickness = 100.f;
};

USTRUCT(BlueprintType)
struct FSurvivalSovietSiteKit
{
	GENERATED_BODY()

	// Spawned as a ground spline per street (runtime ribbon that hugs the ground).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roads")
	TSoftClassPtr<AActor> MainRoadClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roads")
	TSoftClassPtr<AActor> LaneRoadClass;

	// Wide surface of an airstrip.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roads")
	TSoftClassPtr<AActor> StripRoadClass;

	// Decal surfaces (instances of M_SovietGroundDecal): soft verges, wheel ruts, faded ends and filled junctions. A kind
	// without one falls back to its road class above.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roads")
	TSoftObjectPtr<UMaterialInterface> MainRoadSurface;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roads")
	TSoftObjectPtr<UMaterialInterface> LaneRoadSurface;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roads")
	TSoftObjectPtr<UMaterialInterface> TrackRoadSurface;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roads")
	TSoftObjectPtr<UMaterialInterface> StripRoadSurface;

	// Hard standing that runs through every gate and fades out on both sides.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roads")
	TSoftObjectPtr<UMaterialInterface> GateApronSurface;

	// Painted width against the street's planned width, per kind: the main street reads clearly wider than the lanes, while
	// the ground kept free for a street stays as planned.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roads", meta = (ClampMin = "0.2"))
	float MainRoadPaintShare = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roads", meta = (ClampMin = "0.2"))
	float LaneRoadPaintShare = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roads", meta = (ClampMin = "0.2"))
	float TrackRoadPaintShare = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter")
	TArray<FSurvivalSovietWallFamily> WallFamilies;

	// Berms and outposts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter")
	TArray<FEnemyBlueprintEntry> SandbagRuns;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter")
	TArray<FEnemyBlueprintEntry> BarbedWire;

	// Short blocks either side of a gate in a light perimeter.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter")
	TArray<FEnemyBlueprintEntry> GateBlocks;

	// U-shaped vehicle revetment, opening along local +X.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle Park")
	TArray<FEnemyBlueprintEntry> Revetments;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle Park")
	TArray<FEnemyBlueprintEntry> CamoNets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle Park")
	TArray<FEnemyBlueprintEntry> Wrecks;

	// Fill the rows of a block next to the real buildings.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camp")
	TArray<FEnemyBlueprintEntry> Huts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camp")
	TArray<FEnemyBlueprintEntry> Hangars;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camp")
	TArray<FEnemyBlueprintEntry> Flags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stores")
	TArray<FEnemyBlueprintEntry> Crates;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stores")
	TArray<FEnemyBlueprintEntry> Barrels;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stores")
	TArray<FEnemyBlueprintEntry> Munitions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stores")
	TArray<FEnemyBlueprintEntry> Materials;

	// Large camouflage nets spanned over the stacks of an ammunition yard.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stores")
	TArray<FEnemyBlueprintEntry> YardNets;

	// Every storage yard is closed by one destructible fence family with an entrance towards its street.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stores")
	ESurvivalSovietWallFamily AmmoYardFence = ESurvivalSovietWallFamily::PostApoConcrete;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stores")
	ESurvivalSovietWallFamily FuelYardFence = ESurvivalSovietWallFamily::ChainLink;

	// Lamp posts beside gates, junctions and yard entrances: an ASurvivalSovietBaseLantern with this mesh.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stores")
	FEnemyBlueprintEntry Lantern;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stores")
	TSoftObjectPtr<UStaticMesh> LanternMesh;

	// The actor of a ground pad; see ASurvivalSovietBasePad.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foundations")
	TSoftClassPtr<AActor> PadClass;

	// Decal materials by surface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foundations")
	TSoftObjectPtr<UMaterialInterface> ConcretePad;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foundations")
	TSoftObjectPtr<UMaterialInterface> GravelPad;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foundations")
	TSoftObjectPtr<UMaterialInterface> PackedEarthPad;

	// How far a pad reaches past the footprint on every side, cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foundations", meta = (ClampMin = "0.0"))
	float PadMargin = 170.f;

	// Footprints smaller than this half extent get no pad, cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foundations", meta = (ClampMin = "0.0"))
	float PadMinHalfExtent = 200.f;

	// The pit meshes paint their earth with a virtual texture material that only works on maps with that texture.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foundations")
	TSoftObjectPtr<UMaterialInterface> VirtualTextureEarth;

	// Replaces VirtualTextureEarth on spawned structures.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foundations")
	TSoftObjectPtr<UMaterialInterface> EarthStandIn;
};

/**
 * @brief One resource of a base's payout; the payout grows with the base's real threat (see RewardPerThreatPoint).
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietBaseReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	ERTSResourceType Resource = ERTSResourceType::Resource_Radixite;

	// Share of the payout paid in this resource; shares of a preset should add up to 1.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Share = 0.5f;
};

/**
 * @brief One kind of Soviet base. A preset is picked (seeded, weighted) per base among those allowed for the
 * base's strength; the strength then decides which slots, lines and garrison groups appear.
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietBasePreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FName PresetName = NAME_None;

	// Shown in the "base destroyed" notification.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FText DisplayName;

	// The street plan, perimeter and zoning this preset's buildings are laid out on.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	ESurvivalSovietBaseArchetype Archetype = ESurvivalSovietBaseArchetype::Firebase;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0"))
	int32 MinStrength = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0"))
	int32 MaxStrength = 2;

	// A difficulty capped below this tier never rolls this preset: a tank park without a factory is no tank park.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0"))
	int32 MinProductionTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TArray<FSurvivalSovietStructureSlot> ProductionBuildings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Support")
	TArray<FSurvivalSovietStructureSlot> SupportBuildings;

	// Hedgehog belt in front of threatened runs; the difficulty decides whether it is laid.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter")
	FEnemyObstacleBeltSettings ObstacleBelt;

	// The archetype's own scaling order: guns, bunkers and units bought with the base's threat budget.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Threat Path")
	TArray<FSurvivalSovietThreatStep> ThreatPath;

	// Ridges and ramp heads farther than this fraction of the build area radius from the centre are ignored.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Threat Path", meta = (ClampMin = "0.5"))
	float MaxDefensePointDistanceFraction = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TArray<FSurvivalSovietBaseReward> DestroyRewards;
};

/**
 * @brief How one difficulty scales every base.
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietBaseDifficultyScaling
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;

	// The named character of this difficulty, for the designer; not shown in game.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FString Character;

	// Strength of a base by its distance rank from the player, nearest first; the table is stretched over the number
	// of bases. Each difficulty has its own spread, which is what makes its map differ before any number does.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	TArray<int32> StrengthByDistanceRank = {0, 0, 1, 2};

	// Total threat points a base may buy, by distance rank, nearest first: what the player meets in the order he meets
	// it. Strength only picks plans and content.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	TArray<int32> ThreatBudgetByDistanceRank = {65, 110, 170, 310};

	// Part of the budget a base may spend on strategic AI reserves, by distance rank: the mobile force the AI has from
	// the first second, so it stays near the budget of the first waves over the whole map.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	TArray<int32> ReservePointsByDistanceRank = {0, 0, 7, 20};

	// Most expensive single unit fielded, in AI training points: the garrison's tech ceiling.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "1"))
	int32 MaxUnitPoints = 15;

	// Points that structures handed back buy garrison again, but only units at least this expensive: the tier's own
	// force, not a crowd of its cheapest vehicles.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "1"))
	int32 MinTopUpUnitPoints = 3;

	// Highest production tier spawned (0 = barracks only); caps the tech level the enemy AI can reach.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0"))
	int32 MaxProductionTier = 1;

	// Parallel wire runs outside threatened fence runs, by strength; other runs get at most one.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	TArray<int32> WireRowsByStrength = {0, 1, 1};

	// Hedgehog belts are laid on bases at least this strong; above the highest strength means never.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0"))
	int32 BeltMinStrength = 1;

	// Depth: outposts ahead of the line on the open approaches, by strength. Each takes a bunker of the threat path.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty|Depth")
	TArray<int32> ForwardPostsByStrength = {0, 0, 0};

	// Depth: bunkers of a second row beyond the wall, by strength. Bunkers never stand inside a base.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty|Depth")
	TArray<int32> SecondRowBunkersByStrength = {0, 0, 0};

	// Depth: a second hedgehog belt further out on threatened runs.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty|Depth")
	bool bSecondBelt = false;

	// Weight per preset for the farthest base only; 0 rules a preset out there. Presets not listed keep their weight.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	TMap<FName, float> FarthestBasePresetWeights;

	// Resource points paid per threat point of the base when it is destroyed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0"))
	float RewardPerThreatPoint = 6.f;
};
