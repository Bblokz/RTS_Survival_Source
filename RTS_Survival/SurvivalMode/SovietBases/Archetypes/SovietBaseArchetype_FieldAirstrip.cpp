// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePlanPen.h"
#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePresetLibrary.h"

#define LOCTEXT_NAMESPACE "SurvivalSovietBaseSettings"

using namespace SovietBaseArchetypeConstants;

// Airstrip: a long strip along one chord, an apron with hangars, the camp behind it.
void SovietPlan_FieldAirstrip(const FSovietPlanPen& Pen)
{
	const double StripX = Pen.Random.FRandRange(-2800.0, -2200.0);
	// The strip stops 700 short of the fence's flat (7900 x cos 22.5 = 7300): a runway through the wire would leave open fence
	// slots that lead into the outer obstacle belt.
	const double StripHalf = 6600.0;
	const double ApronX = StripX + 3000.0;
	const double CampX = ApronX + 2300.0;

	Pen.Street(ESurvivalSovietRoadKind::Strip, StripWidth, {FVector2D(StripX, -StripHalf), FVector2D(StripX, StripHalf)});
	Pen.Main({FVector2D(7600.0 + RoadPastGate, 0.0), FVector2D(StripX + StripWidth * 0.5, 0.0)});
	Pen.Lane({FVector2D(ApronX, -5000.0), FVector2D(ApronX, 5000.0)});
	Pen.Gate(FVector2D(7600.0, 0.0), FVector2D(1.0, 0.0), false);
	Pen.FenceLoop(SovietPlan_Polygon(8, 7900.0, PI / 8.0), ESurvivalSovietWallFamily::WireFence);

	for (const double Sign : {1.0, -1.0})
	{
		FSovietPlot& Hangars = Pen.Plot(ESurvivalSovietZone::Workshop, FVector2D(ApronX - 1520.0, Sign * 2950.0),
		                                FVector2D(-1.0, 0.0), 2300.0, 950.0);
		Hangars.Dressing = ESovietPlotDressing::Hangars;
		Hangars.Gap = 400.0;
		FSovietPlot& Camp = Pen.Plot(Sign > 0.0 ? ESurvivalSovietZone::Barracks : ESurvivalSovietZone::Depot,
		                             FVector2D(CampX, Sign * 2800.0), FVector2D(-1.0, 0.0), 2200.0, 1700.0);
		Camp.Gap = 480.0;
		Camp.Dressing = Sign > 0.0 ? ESovietPlotDressing::Huts : ESovietPlotDressing::Crates;
	}
	FSovietPlot& Command = Pen.Plot(ESurvivalSovietZone::Command, FVector2D(CampX + 1800.0, 3400.0), FVector2D(0.0, -1.0),
	                                1000.0, 900.0);
	Command.Gap = 400.0;
	const double FarSide = Pen.Chance(0.5f) ? 1.0 : -1.0;
	// Fuel and bombs sit on the two halves behind the strip with a fire break between them: their fences stand about
	// 2800 apart, so no fuel stack is within 2000 of the bombs.
	FSovietPlot& Fuel = Pen.Plot(ESurvivalSovietZone::Fuel, FVector2D(StripX - 2300.0, FarSide * 3300.0),
	                             FVector2D(1.0, 0.0), 1700.0, 1000.0);
	Fuel.bBerm = true;
	Fuel.Gap = 360.0;
	Fuel.Dressing = ESovietPlotDressing::Barrels;
	FSovietPlot& Bombs = Pen.Plot(ESurvivalSovietZone::Depot, FVector2D(StripX - 2300.0, -FarSide * 3100.0),
	                              FVector2D(1.0, 0.0), 1500.0, 900.0);
	Bombs.Dressing = ESovietPlotDressing::Munitions;

	// AA at both ends of the strip and on the apron.
	for (const double Sign : {1.0, -1.0})
	{
		Pen.Gun(ESurvivalSovietZone::AirDefense, FVector2D(StripX + 1700.0, Sign * 6300.0));
		Pen.Gun(ESurvivalSovietZone::AirDefense, FVector2D(StripX - 1700.0, Sign * 6300.0));
		Pen.Gun(ESurvivalSovietZone::AntiTank, FVector2D(StripX + 2900.0, Sign * 6000.0));
		Pen.Gun(ESurvivalSovietZone::AntiTank, FVector2D(5800.0, Sign * 4900.0));
		Pen.Gun(ESurvivalSovietZone::Artillery, FVector2D(5400.0, Sign * 1400.0));
		Pen.Gun(ESurvivalSovietZone::Watch, FVector2D(ApronX + 600.0, Sign * 700.0));
	}
	Pen.Gun(ESurvivalSovietZone::AntiTank, FVector2D(StripX - 1700.0, 500.0));
	Pen.Gun(ESurvivalSovietZone::AirDefense, FVector2D(StripX - 1700.0, -500.0));
	for (const FVector2D& Corner : SovietPlan_Polygon(8, 7300.0, PI / 8.0))
	{
		Pen.Gun(ESurvivalSovietZone::Watch, Corner);
	}
	Pen.Plan.InteriorRadius = 6800.0;
	Pen.Plan.OuterRadius = 7900.0;
}

// Field Airstrip adds air defence: everything protects the strip.
void FSovietPresetLibrary::AddFieldAirstrip(TArray<FSurvivalSovietBasePreset>& Presets) const
{
	using namespace SurvivalSovietBaseDefaults;
	FSurvivalSovietBasePreset& Preset = AddPreset(Presets, TEXT("FieldAirstrip"), LOCTEXT("FieldAirstrip", "Field Airstrip"),
	                                              EArch::Airstrip, 0, 2, 0);
	Preset.ProductionBuildings = {Barracks, Tent, Cooling, Research};
	Preset.SupportBuildings = {
		MakeSlot(EZone::Fuel, RusBuildings, TEXT("Rus_Building_FuelStorageTank"), 0, -1, 1, 0.5f, EDiff::NewToRTS, 0.34f),
		Ammo, Supplies
	};
	Preset.ObstacleBelt = MakeDefaultBelt(EEnemyObstacleFormation::StraightLine);
	Preset.ThreatPath = {
		PitDShK, Later(Tower, 1), Hold(Rifles, 8), Signature(PitBofors, 0), Pit53K, Patrol(Scouts, 3),
		Gated(Hold(AATanks, 3), EDiff::Hard, 1, 2), PitAmpulomet, Later(PitMortar, 1), Later(MGBunker, 1), Later(PitBS3, 1),
		Later(AutoCn, 2), Gated(Patrol(LightTanks, 4), EDiff::Normal, 0, 1), Gated(Patrol(MediumTanks, 3), EDiff::Hard, 1, 2),
		Signature(PitML20), Signature(PitB4), Later(Bunker03, 2), Later(Platform, 2), Gated(Hold(HeavyTanks, 3), EDiff::Brutal, 2, 2)
	};
	Preset.ThreatPath.Append(Reserves());
	Preset.DestroyRewards = InfantryReward;
}

#undef LOCTEXT_NAMESPACE
