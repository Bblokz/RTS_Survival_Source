// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePlanPen.h"
#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePresetLibrary.h"

#define LOCTEXT_NAMESPACE "SurvivalSovietBaseSettings"

using namespace SovietBaseArchetypeConstants;

namespace
{
	void SovietPlan_CantonmentYards(const FSovietPlanPen& Pen, const double Back, const double Lane, const double Edge)
	{
		const double YardCenter = (Lane + Edge) * 0.5 + 150.0;
		const double YardDepth = (Edge - Lane) * 0.5 - 600.0;
		const double Side = Pen.Chance(0.5f) ? 1.0 : -1.0;
		FSovietPlot& Depot = Pen.Plot(ESurvivalSovietZone::Depot, FVector2D(-2500.0, Side * YardCenter),
		                              FVector2D(0.0, -Side), 2700.0, YardDepth);
		Depot.Dressing = ESovietPlotDressing::Crates;
		FSovietPlot& Huts = Pen.Plot(ESurvivalSovietZone::Barracks, FVector2D(2900.0, Side * YardCenter),
		                             FVector2D(0.0, -Side), 2400.0, YardDepth);
		Huts.Dressing = ESovietPlotDressing::Huts;
		FSovietPlot& Fuel = Pen.Plot(ESurvivalSovietZone::Fuel, FVector2D(3700.0, -Side * YardCenter),
		                             FVector2D(0.0, Side), 1700.0, YardDepth);
		Fuel.Dressing = ESovietPlotDressing::Barrels;
		Fuel.bBerm = true;
		FSovietPlot& Workshop = Pen.Plot(ESurvivalSovietZone::Workshop, FVector2D(-1500.0, -Side * YardCenter),
		                                 FVector2D(0.0, Side), 3600.0, YardDepth);
		Workshop.Dressing = ESovietPlotDressing::Workshop;
		Workshop.Gap = 500.0;
	}
}

// Cantonment: parade axis, rows of barracks, service yards behind the lanes.
void SovietPlan_RifleCantonment(const FSovietPlanPen& Pen)
{
	const double Front = Pen.Size(5900.0, 6500.0);
	const double Side = Pen.Size(4700.0, 5100.0);
	const double Lane = 3100.0;
	const double Back = -Front;
	const double ParadeLine = Pen.Random.FRandRange(-2700.0, -2100.0);
	const double MidLine = Pen.Random.FRandRange(2300.0, 3000.0);

	Pen.Main({FVector2D(Front + RoadPastGate, 0.0), FVector2D(ParadeLine, 0.0)});
	Pen.Lane({FVector2D(ParadeLine, -Lane), FVector2D(ParadeLine, Lane)});
	Pen.Lane({FVector2D(MidLine, -Lane), FVector2D(MidLine, Lane)});
	Pen.Lane({FVector2D(Back + 700.0, Lane), FVector2D(Front - 700.0, Lane)});
	Pen.Lane({FVector2D(Back + 700.0, -Lane), FVector2D(Front - 700.0, -Lane)});
	// The parade square stays empty.
	Pen.Street(ESurvivalSovietRoadKind::None, 3400.0, {FVector2D(ParadeLine, 0.0), FVector2D(ParadeLine - 1900.0, 0.0)});
	Pen.Gate(FVector2D(Front, 0.0), FVector2D(1.0, 0.0), false);
	Pen.FenceLoop({
		FVector2D(Front, Side), FVector2D(Back, Side), FVector2D(Back, -Side), FVector2D(Front, -Side)
	}, ESurvivalSovietWallFamily::WireFence);

	FSovietPlot& Command = Pen.Plot(ESurvivalSovietZone::Command, FVector2D(ParadeLine - 2700.0, 0.0),
	                                FVector2D(1.0, 0.0), 2400.0, 800.0);
	Command.Dressing = ESovietPlotDressing::Parade;

	for (const double Sign : {1.0, -1.0})
	{
		const double BlockDepth = (Lane - 500.0 - 500.0) * 0.5;
		const double BlockY = Sign * (525.0 + BlockDepth);
		const double FirstLength = (MidLine - ParadeLine - 900.0) * 0.5;
		FSovietPlot& First = Pen.Plot(ESurvivalSovietZone::Barracks, FVector2D((MidLine + ParadeLine) * 0.5, BlockY),
		                              FVector2D(0.0, -Sign), FirstLength, BlockDepth);
		First.Facing = ESovietPlotFacing::GableToStreet;
		First.Dressing = ESovietPlotDressing::Huts;
		First.Gap = 520.0;
		const double SecondLength = (Front - 800.0 - MidLine - 500.0) * 0.5;
		FSovietPlot& Second = Pen.Plot(ESurvivalSovietZone::Barracks,
		                               FVector2D(MidLine + 500.0 + SecondLength, BlockY), FVector2D(0.0, -Sign),
		                               SecondLength, BlockDepth);
		Second.Facing = ESovietPlotFacing::GableToStreet;
		Second.Dressing = ESovietPlotDressing::Huts;
		Second.Gap = 520.0;
	}
	SovietPlan_CantonmentYards(Pen, Back, Lane, Side);

	for (const FVector2D& Corner : {
		     FVector2D(Front, Side), FVector2D(Front, -Side), FVector2D(Back, Side), FVector2D(Back, -Side)
	     })
	{
		Pen.Gun(ESurvivalSovietZone::Watch, Corner * 0.93);
	}
	Pen.Gun(ESurvivalSovietZone::Watch, FVector2D(Front - 900.0, 1500.0));
	Pen.Gun(ESurvivalSovietZone::Watch, FVector2D(Front - 900.0, -1500.0));
	for (const FVector2D& Pit : {
		     FVector2D(7400.0, 2600.0), FVector2D(7400.0, -2600.0), FVector2D(-7300.0, 2300.0),
		     FVector2D(-7300.0, -2300.0), FVector2D(0.0, 6300.0), FVector2D(0.0, -6300.0),
		     FVector2D(3700.0, 6200.0), FVector2D(3700.0, -6200.0), FVector2D(-3700.0, 6200.0),
		     FVector2D(-3700.0, -6200.0)
	     })
	{
		Pen.Gun(ESurvivalSovietZone::AntiTank, Pit);
		Pen.Gun(ESurvivalSovietZone::Artillery, Pit);
		Pen.Gun(ESurvivalSovietZone::AirDefense, Pit);
	}
	Pen.Plan.InteriorRadius = Side - 500.0;
	Pen.Plan.OuterRadius = Front;
}

// Rifle Cantonment adds infantry and barracks: a garrison town, thin in heavy weapons.
void FSovietPresetLibrary::AddRifleCantonment(TArray<FSurvivalSovietBasePreset>& Presets) const
{
	using namespace SurvivalSovietBaseDefaults;
	FSurvivalSovietBasePreset& Preset = AddPreset(Presets, TEXT("RifleCantonment"), LOCTEXT("RifleCantonment", "Rifle Cantonment"),
	                                              EArch::Cantonment, 0, 1, 0);
	FSurvivalSovietStructureSlot Headquarters = Barracks;
	Headquarters.Zone = EZone::Command;
	FSurvivalSovietStructureSlot MoreBarracks = Barracks;
	MoreBarracks.CountPerStrength = 1.f;
	MoreBarracks.CountPerDifficulty = 0.34f;
	Preset.ProductionBuildings = {Headquarters, Tent, MoreBarracks, Cooling};
	Preset.SupportBuildings = {Supplies, Ammo, Fuel, Speaker};
	Preset.ObstacleBelt = MakeDefaultBelt(EEnemyObstacleFormation::StraightLine);
	Preset.ThreatPath = {
		Tower, Hold(Rifles, 14), Hold(Hunters, 4), Later(Pit53K, 1), Gated(Hold(TankHunters, 6), EDiff::Normal),
		Later(PitDShK, 1), Later(PitMortar, 2), Later(MGBunker, 1), Later(AutoCn, 2), Signature(ZisBunker, 0),
		Patrol(Scouts, 3), Signature(Gated(Hold(Guards, 8), EDiff::Hard)), Gated(Patrol(LightTanks, 2), EDiff::Normal, 0, 1),
		Later(PitBofors, 2), Later(PitBS3, 2), Signature(Bunker03), Signature(Gated(Hold(Guards, 8), EDiff::Ironman, 0)),
		Gated(Hold(MediumTanks, 3), EDiff::Hard, 1, 2), Later(Bunker05, 2)
	};
	Preset.ThreatPath.Append(Reserves());
	Preset.DestroyRewards = InfantryReward;
}

#undef LOCTEXT_NAMESPACE
