// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseLayout.h"

/** @brief Whether a building shows its long side or its door end to the street. */
enum class ESovietPlotFacing : uint8
{
	LongSideToStreet,
	GableToStreet
};

/** @brief What the free ground of a plot is filled with once its buildings stand. */
enum class ESovietPlotDressing : uint8
{
	None,
	Huts,
	Crates,
	Munitions,
	Barrels,
	Workshop,
	Parade,
	Hangars
};

/** @brief Where the main gate goes relative to the player; the terrain then picks the nearest open approach. */
enum class ESovietGatePreference : uint8
{
	TowardThreat,
	AwayFromThreat,
	FlankThreat
};

/*
 * Every position of a master plan is in the base's local frame: +X points out of the main gate, +Y to its left, in cm
 * on a build area of radius 8000. FSovietLayoutContext scales and rotates them into the world, and mirrors them per
 * seed, so an archetype is one plan drawn once.
 */

struct FSovietStreet
{
	ESurvivalSovietRoadKind Kind = ESurvivalSovietRoadKind::Main;
	TArray<FVector2D> Points;
	double Width = 900.0;
};

/** @brief A block of ground beside a street; buildings stand in rows, the first row on the street side. */
struct FSovietPlot
{
	ESurvivalSovietZone Zone = ESurvivalSovietZone::Depot;
	FVector2D Center = FVector2D::ZeroVector;
	// Unit vector from the plot toward its street.
	FVector2D Front = FVector2D(1.0, 0.0);
	double HalfLength = 1000.0;
	double HalfDepth = 500.0;
	ESovietPlotFacing Facing = ESovietPlotFacing::LongSideToStreet;
	ESovietPlotDressing Dressing = ESovietPlotDressing::None;
	// Free ground between two buildings of a row and between rows.
	double Gap = 650.0;
	// Sandbag berm on the three sides away from the street.
	bool bBerm = false;
};

struct FSovietFenceRun
{
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D End = FVector2D::ZeroVector;
};

struct FSovietGate
{
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Outward = FVector2D(1.0, 0.0);
	bool bArch = false;
};

/** @brief A spot a gun or tower of this zone may take; a taken or unusable spot is skipped. */
struct FSovietEmplacement
{
	ESurvivalSovietZone Zone = ESurvivalSovietZone::AntiTank;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Facing = FVector2D(1.0, 0.0);
};

/** @brief A row of vehicle revetments whose openings face a lane. */
struct FSovietParkRow
{
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D End = FVector2D::ZeroVector;
	FVector2D Front = FVector2D(1.0, 0.0);
};

/** @brief What an archetype draws: the executor in FSovietLayoutContext turns it into structures. */
struct FSovietMasterPlan
{
	TArray<FSovietStreet> Streets;
	TArray<FSovietPlot> Plots;
	TArray<FSovietFenceRun> Fence;
	// One family per base: every run of Fence is built from it.
	ESurvivalSovietWallFamily WallFamily = ESurvivalSovietWallFamily::None;
	// Closed polygon: what lies beyond it is outside the base. Bunkers may only stand outside (validated, see
	// FSovietLayoutContext::ValidateBunkerRule). FenceLoop fills it; a plan without a closed wall must set it itself.
	TArray<FVector2D> PerimeterLine;
	// Surface under the buildings of a zone; a zone not listed here gets FSovietMasterPlan::DefaultPad.
	TMap<ESurvivalSovietZone, ESurvivalSovietPadKind> PadKinds;
	ESurvivalSovietPadKind DefaultPad = ESurvivalSovietPadKind::Gravel;
	TArray<FSovietGate> Gates;
	TArray<FSovietEmplacement> Emplacements;
	TArray<FSovietParkRow> ParkRows;

	// Share of the direct fire and AA guns sent out to ridges and ramp heads.
	double RidgeGunShare = 0.34;
	// Everything inside this radius counts as inside the perimeter; guns that find no listed spot search in it.
	double InteriorRadius = 5000.0;
	// Where the main line runs at its farthest; forward posts stand ahead of it.
	double OuterRadius = 6000.0;
	// Every run gets line works, not only the threatened ones.
	bool bFortifyAllRuns = false;
};

/** @return Where the archetype wants its main gate. */
ESovietGatePreference GetSovietGatePreference(ESurvivalSovietBaseArchetype Archetype, FRandomStream& Random);

/**
 * @brief Draws the archetype's master plan for this strength and seed.
 * @param SizeFill 0..1, how much of its size range the plan uses: a bigger budget needs a bigger base.
 * @param Site Terrain knowledge; strongholds shape their lines by it. May be null.
 * @param ToLocal Converts a world position to the plan's local frame.
 */
FSovietMasterPlan BuildSovietMasterPlan(
	ESurvivalSovietBaseArchetype Archetype,
	int32 Strength,
	double SizeFill,
	FRandomStream& Random,
	const FSurvivalSovietSiteMap* Site,
	const TFunction<FVector2D(const FVector2D&)>& ToLocal,
	const TFunction<FVector2D(const FVector2D&)>& ToWorld,
	const TArray<FVector2D>& DefensePointsLocal);
