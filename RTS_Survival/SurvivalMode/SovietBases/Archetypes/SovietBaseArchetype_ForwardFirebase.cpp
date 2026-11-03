// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePlanPen.h"
#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePresetLibrary.h"

#define LOCTEXT_NAMESPACE "SurvivalSovietBaseSettings"

using namespace SovietBaseArchetypeConstants;

// Firebase: a wheel. Rim of sandbags and wire, ring road, guns between ring and rim, hub with command.
void SovietPlan_ForwardFirebase(const FSovietPlanPen& Pen)
{
	const int32 Sides = Pen.Chance(0.5f) ? 8 : 6;
	const double SideYaw = 2.0 * PI / Sides;
	const double Rim = Pen.Size(5300.0, 7200.0) + Pen.Random.FRandRange(-120.0, 120.0);
	const double Ring = Rim * 0.56;
	const double RimFlat = Rim * FMath::Cos(SideYaw * 0.5);
	const double RingFlat = Ring * FMath::Cos(SideYaw * 0.5);

	Pen.FenceLoop(SovietPlan_Polygon(Sides, Rim, SideYaw * 0.5), ESurvivalSovietWallFamily::Sandbag);
	TArray<FVector2D> RingPoints = SovietPlan_Polygon(Sides, Ring, SideYaw * 0.5);
	const FVector2D FirstRingPoint = RingPoints[0];
	RingPoints.Add(FirstRingPoint);
	Pen.Lane(RingPoints);
	Pen.Main({FVector2D(RimFlat + RoadPastGate, 0.0), FVector2D(RingFlat, 0.0)});
	Pen.Lane({FVector2D(RingFlat, 0.0), FVector2D(-RingFlat, 0.0)});
	Pen.Gate(FVector2D(RimFlat, 0.0), FVector2D(1.0, 0.0), false);

	// Hub: the two blocks either side of the spine.
	const double HubDepth = (RingFlat - 950.0) * 0.5;
	for (const double Sign : {1.0, -1.0})
	{
		FSovietPlot& Hub = Pen.Plot(Sign > 0.0 ? ESurvivalSovietZone::Barracks : ESurvivalSovietZone::Command,
		                            FVector2D(0.0, Sign * (475.0 + HubDepth)), FVector2D(0.0, -Sign),
		                            RingFlat * 0.62, HubDepth);
		Hub.Gap = 450.0;
	}

	// The two sides next to the gate hold the ammunition, close to the road and away from the threat.
	const double Band = (Ring + Rim) * 0.5;
	for (const double Sign : {1.0, -1.0})
	{
		const FVector2D Center = SovietPlan_Polar(Band * FMath::Cos(SideYaw * 0.5), Sign * SideYaw);
		FSovietPlot& Store = Pen.Plot(ESurvivalSovietZone::Depot, Center, -Center.GetSafeNormal(),
		                              Rim * 0.3, (Rim - Ring) * 0.36);
		Store.Dressing = ESovietPlotDressing::Munitions;
		Store.Gap = 300.0;
	}

	for (int32 Side = 2; Side <= Sides - 2; ++Side)
	{
		const FVector2D Pit = SovietPlan_Polar(Band * FMath::Cos(SideYaw * 0.5), Side * SideYaw);
		Pen.Gun(ESurvivalSovietZone::AntiTank, Pit);
		Pen.Gun(ESurvivalSovietZone::Artillery, Pit);
		Pen.Gun(ESurvivalSovietZone::AirDefense, Pit);
	}
	for (int32 Corner = 0; Corner < Sides; ++Corner)
	{
		Pen.Gun(ESurvivalSovietZone::Watch, SovietPlan_Polar(Rim * 0.9, (Corner + 0.5) * SideYaw));
	}
	// Guns also stand on the spokes between the sides, so a bigger budget means a denser rim.
	for (int32 Corner = 1; Corner < Sides - 1; ++Corner)
	{
		const FVector2D Pit = SovietPlan_Polar(Band, (Corner + 0.5) * SideYaw);
		Pen.Gun(ESurvivalSovietZone::AntiTank, Pit);
		Pen.Gun(ESurvivalSovietZone::Artillery, Pit);
	}
	Pen.Plan.InteriorRadius = RimFlat - 700.0;
	Pen.Plan.OuterRadius = Rim;
	Pen.Plan.bFortifyAllRuns = true;
}

// Forward Firebase adds guns: every level of threat is first another pit on the rim.
void FSovietPresetLibrary::AddForwardFirebase(TArray<FSurvivalSovietBasePreset>& Presets) const
{
	using namespace SurvivalSovietBaseDefaults;
	FSurvivalSovietBasePreset& Preset = AddPreset(Presets, TEXT("ForwardFirebase"), LOCTEXT("ForwardFirebase", "Forward Firebase"),
	                                              EArch::Firebase, 0, 1, 0);
	Preset.ProductionBuildings = {Barracks, Tent, Cooling};
	Preset.SupportBuildings = {Ammo, Speaker};
	Preset.ObstacleBelt = MakeDefaultBelt(EEnemyObstacleFormation::StraightLine);
	Preset.ThreatPath = {
		Pit53K, Tower, PitMortar, PitDShK, Hold(Hunters, 3), Hold(Rifles, 6),
		Signature(PitBofors, 0), Signature(PitBS3), Signature(PitML20), Signature(PitB4), Later(MGBunker, 1), Later(AutoCn, 1),
		Later(Bunker03, 2), Patrol(Scouts, 2), Gated(Patrol(LightTanks, 2), EDiff::Normal, 0, 1),
		Gated(Hold(MediumTanks, 3), EDiff::Hard, 1, 2), Gated(Hold(Guards, 6), EDiff::Brutal)
	};
	Preset.ThreatPath.Append(Reserves());
	Preset.DestroyRewards = InfantryReward;
}

#undef LOCTEXT_NAMESPACE
