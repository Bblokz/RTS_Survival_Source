// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseMasterPlan.h"

/*
 * Shared by the archetype files. A base type owns only its own Archetypes/SovietBaseArchetype_<Type>.cpp; see
 * Saved/AgentNotes/base_types_contract.md and Docs/50_SovietBases.md.
 */

namespace SovietBaseArchetypeConstants
{
	constexpr double MainStreetWidth = 1050.0;
	constexpr double LaneWidth = 850.0;
	// Two-track to an outlying post; painted narrower still (see FSurvivalSovietSiteKit paint shares).
	constexpr double TrackWidth = 700.0;
	constexpr double StripWidth = 1500.0;
	// The road runs on past the gate so the approach reads and belts keep clear of it.
	constexpr double RoadPastGate = 2300.0;
	constexpr int32 MaxPlanStrength = 2;
}

/** @brief Pen the archetypes draw with; keeps each plan a readable list of streets, plots and guns. */
struct FSovietPlanPen
{
	FSovietMasterPlan& Plan;
	FRandomStream& Random;
	int32 Strength = 0;
	// 0..1: where the plan sits in its size range.
	double Fill = 0.0;
	// World cm per plan unit (build radius / 8 000). Buildings keep their size when it drops, so a plan may draw a compact
	// variant below 1.
	double Unit = 1.0;

	double Size(const double Smallest, const double Largest) const
	{
		return FMath::Lerp(Smallest, Largest, Fill);
	}

	void Street(const ESurvivalSovietRoadKind Kind, const double Width, const TArray<FVector2D>& Points) const
	{
		FSovietStreet& Street = Plan.Streets.AddDefaulted_GetRef();
		Street.Kind = Kind;
		Street.Width = Width;
		Street.Points = Points;
	}

	void Main(const TArray<FVector2D>& Points) const
	{
		Street(ESurvivalSovietRoadKind::Main, SovietBaseArchetypeConstants::MainStreetWidth, Points);
	}

	void Lane(const TArray<FVector2D>& Points) const
	{
		Street(ESurvivalSovietRoadKind::Lane, SovietBaseArchetypeConstants::LaneWidth, Points);
	}

	void Track(const TArray<FVector2D>& Points) const
	{
		Street(ESurvivalSovietRoadKind::Track, SovietBaseArchetypeConstants::TrackWidth, Points);
	}

	FSovietPlot& Plot(const ESurvivalSovietZone Zone, const FVector2D& Center, const FVector2D& Front,
	                  const double HalfLength, const double HalfDepth) const
	{
		FSovietPlot& Plot = Plan.Plots.AddDefaulted_GetRef();
		Plot.Zone = Zone;
		Plot.Center = Center;
		Plot.Front = Front;
		Plot.HalfLength = HalfLength;
		Plot.HalfDepth = HalfDepth;
		return Plot;
	}

	void Gun(const ESurvivalSovietZone Zone, const FVector2D& Position) const
	{
		FSovietEmplacement& Emplacement = Plan.Emplacements.AddDefaulted_GetRef();
		Emplacement.Zone = Zone;
		Emplacement.Position = Position;
		Emplacement.Facing = Position.GetSafeNormal();
	}

	void Gate(const FVector2D& Position, const FVector2D& Outward, const bool bArch) const
	{
		FSovietGate& Gate = Plan.Gates.AddDefaulted_GetRef();
		Gate.Position = Position;
		Gate.Outward = Outward;
		Gate.bArch = bArch;
	}

	/**
	 * @brief Closes a perimeter through Corners, built from one wall family. The polygon is also the line that decides
	 * what counts as outside the base, see FSovietMasterPlan::PerimeterLine.
	 */
	void FenceLoop(const TArray<FVector2D>& Corners, const ESurvivalSovietWallFamily Family) const
	{
		Plan.WallFamily = Family;
		Plan.PerimeterLine = Corners;
		for (int32 Index = 0; Index < Corners.Num(); ++Index)
		{
			FSovietFenceRun& Run = Plan.Fence.AddDefaulted_GetRef();
			Run.Start = Corners[Index];
			Run.End = Corners[(Index + 1) % Corners.Num()];
		}
	}

	bool Chance(const float Probability) const
	{
		return Random.FRand() < Probability;
	}
};


inline TArray<FVector2D> SovietPlan_Polygon(const int32 Sides, const double Radius, const double StartYaw)
{
	TArray<FVector2D> Points;
	for (int32 Side = 0; Side < Sides; ++Side)
	{
		const double Yaw = StartYaw + 2.0 * PI * Side / Sides;
		Points.Add(FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw)) * Radius);
	}
	return Points;
}

inline FVector2D SovietPlan_Polar(const double Radius, const double YawRadians)
{
	return FVector2D(FMath::Cos(YawRadians), FMath::Sin(YawRadians)) * Radius;
}

// One per base type.
void SovietPlan_ForwardFirebase(const FSovietPlanPen& Pen);
void SovietPlan_RifleCantonment(const FSovietPlanPen& Pen);
void SovietPlan_TankPark(const FSovietPlanPen& Pen);
void SovietPlan_SupplyDepot(const FSovietPlanPen& Pen);
void SovietPlan_CommandFortress(const FSovietPlanPen& Pen);
void SovietPlan_FieldAirstrip(const FSovietPlanPen& Pen);
void SovietPlan_HillStronghold(
	const FSovietPlanPen& Pen,
	const FSurvivalSovietSiteMap* Site,
	const TFunction<FVector2D(const FVector2D&)>& ToLocal,
	const TFunction<FVector2D(const FVector2D&)>& ToWorld,
	const TArray<FVector2D>& DefensePointsLocal);
