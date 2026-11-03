// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePlanPen.h"
#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePresetLibrary.h"

#define LOCTEXT_NAMESPACE "SurvivalSovietBaseSettings"

using namespace SovietBaseArchetypeConstants;

namespace
{
	void SovietPlan_TankParkRows(const FSovietPlanPen& Pen, const double LaneX, const double RowStart, const double RowEnd)
	{
		constexpr double BayFromLane = 1120.0;
		for (const double Side : {1.0, -1.0})
		{
			for (const double Sign : {1.0, -1.0})
			{
				FSovietParkRow& Row = Pen.Plan.ParkRows.AddDefaulted_GetRef();
				Row.Start = FVector2D(LaneX + Side * BayFromLane, Sign * RowStart);
				Row.End = FVector2D(LaneX + Side * BayFromLane, Sign * RowEnd);
				Row.Front = FVector2D(-Side, 0.0);
			}
		}
	}
}

// Tank park: a boulevard, cross lanes lined with revetments, workshops at the end, fuel off to one flank.
void SovietPlan_TankPark(const FSovietPlanPen& Pen)
{
	const double Front = 6600.0;
	const double Side = 5300.0;
	const double FirstLane = Pen.Random.FRandRange(3700.0, 4200.0);
	const double SecondLane = FirstLane - 3050.0;
	const double EndLane = -3050.0;
	const double LaneReach = 4700.0;

	// Below this scale the park drops its second cross lane and that lane's revetment rows (filler): the ground goes to the
	// production buildings (factory, research workshop, cooling towers), which keep their full size at any build radius.
	constexpr double CompactParkUnit = 0.85;
	const bool bCompact = Pen.Unit < CompactParkUnit;

	Pen.Main({FVector2D(Front + RoadPastGate, 0.0), FVector2D(EndLane, 0.0)});
	Pen.Lane({FVector2D(FirstLane, -LaneReach), FVector2D(FirstLane, LaneReach)});
	if (not bCompact)
	{
		Pen.Lane({FVector2D(SecondLane, -LaneReach), FVector2D(SecondLane, LaneReach)});
	}
	Pen.Lane({FVector2D(EndLane, -LaneReach), FVector2D(EndLane, LaneReach)});
	Pen.Gate(FVector2D(Front, 0.0), FVector2D(1.0, 0.0), false);
	Pen.FenceLoop({
		FVector2D(Front, Side), FVector2D(EndLane - 3350.0, Side), FVector2D(EndLane - 3350.0, -Side),
		FVector2D(Front, -Side)
	}, ESurvivalSovietWallFamily::PostApoConcrete);

	SovietPlan_TankParkRows(Pen, FirstLane, 900.0, LaneReach - 300.0);
	if (not bCompact)
	{
		SovietPlan_TankParkRows(Pen, SecondLane, 900.0, LaneReach - 300.0);
	}

	FSovietPlot& Workshop = Pen.Plot(ESurvivalSovietZone::Workshop, FVector2D(EndLane - 1950.0, 0.0),
	                                 FVector2D(1.0, 0.0), 5000.0, 1500.0);
	Workshop.Gap = 500.0;
	const double YardX = (SecondLane - 1700.0 + EndLane + 450.0) * 0.5;
	const double YardLength = (SecondLane - 1700.0 - EndLane - 450.0) * 0.5;
	FSovietPlot& Barracks = Pen.Plot(ESurvivalSovietZone::Barracks, FVector2D(YardX, 2100.0), FVector2D(0.0, -1.0),
	                                 YardLength, 1500.0);
	Barracks.Dressing = ESovietPlotDressing::Huts;
	Barracks.Gap = 450.0;
	// The compact park keeps the workshop clutter plot out: it is filler, and its ground holds a production building.
	if (not bCompact)
	{
		FSovietPlot& Stores = Pen.Plot(ESurvivalSovietZone::Depot, FVector2D(YardX, 4350.0), FVector2D(0.0, -1.0),
		                               YardLength, 600.0);
		Stores.Dressing = ESovietPlotDressing::Workshop;
	}
	FSovietPlot& Fuel = Pen.Plot(ESurvivalSovietZone::Fuel, FVector2D(YardX, -2800.0), FVector2D(0.0, 1.0),
	                             YardLength - 150.0, 1900.0);
	Fuel.bBerm = true;
	Fuel.Dressing = ESovietPlotDressing::Barrels;
	Fuel.Gap = 420.0;

	for (const FVector2D& Corner : {
		     FVector2D(Front, Side), FVector2D(Front, -Side), FVector2D(-6250.0, Side), FVector2D(-6250.0, -Side)
	     })
	{
		Pen.Gun(ESurvivalSovietZone::Watch, Corner * 0.94);
	}
	for (const FVector2D& Pit : {
		     FVector2D(7500.0, 2600.0), FVector2D(7500.0, -2600.0), FVector2D(2800.0, 6300.0),
		     FVector2D(2800.0, -6300.0), FVector2D(-2200.0, 6300.0), FVector2D(-2200.0, -6300.0),
		     FVector2D(-6900.0, 2600.0), FVector2D(-6900.0, -2600.0)
	     })
	{
		Pen.Gun(ESurvivalSovietZone::AntiTank, Pit);
		Pen.Gun(ESurvivalSovietZone::AirDefense, Pit);
	}
	Pen.Plan.InteriorRadius = Side - 400.0;
	Pen.Plan.OuterRadius = Front;
}

// Tank Park adds armour: more bays filled with heavier tanks, thin fixed defences. Needs a tank factory.
void FSovietPresetLibrary::AddTankPark(TArray<FSurvivalSovietBasePreset>& Presets) const
{
	using namespace SurvivalSovietBaseDefaults;
	FSurvivalSovietBasePreset& Preset = AddPreset(Presets, TEXT("TankPark"), LOCTEXT("TankPark", "Tank Park"), EArch::TankPark, 1, 2, 1);
	FSurvivalSovietStructureSlot Workshops = Research;
	Workshops.Zone = EZone::Workshop;
	Preset.ProductionBuildings = {Factory, Barracks, Tent, Cooling, Workshops};
	Preset.SupportBuildings = {Fuel, FuelTank, Ammo, MakeSlot(EZone::Depot, RusBuildings, TEXT("Rus_Bxp_ContainerHut"), 0, -1, 1, 0.f)};
	Preset.ObstacleBelt = MakeDefaultBelt(EEnemyObstacleFormation::DoubleRow);
	Preset.ThreatPath = {
		Gated(Hold(LightTanks, 14), EDiff::Normal, 0, 1),
		Signature(Gated(Hold(MediumTanks, 10), EDiff::Hard, 0, 2), 0),
		Signature(Gated(Hold(LateMediums, 8), EDiff::Brutal, 0, 2), 0),
		Gated(Hold(HeavyTanks, 8), EDiff::Brutal, 1, 2),
		Signature(Gated(Hold(Breakthrough, 6), EDiff::Ironman, 1, 2)),
		PitSector, Tower, PitDShK, Later(ZisBunker, 1), Later(LongCamo, 1), Later(MGBunker, 1), Later(PitBS3, 1), Later(PitBofors, 1),
		Gated(Hold(TankHunters, 6), EDiff::Normal), Gated(Patrol(LightTanks, 3), EDiff::Normal, 0, 1),
		Gated(Hold(AATanks, 2), EDiff::Hard, 2, 2)
	};
	Preset.ThreatPath.Append(Reserves());
	Preset.DestroyRewards = ArmourReward;
}

#undef LOCTEXT_NAMESPACE
