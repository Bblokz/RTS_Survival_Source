// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePlanPen.h"
#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePresetLibrary.h"

#define LOCTEXT_NAMESPACE "SurvivalSovietBaseSettings"

using namespace SovietBaseArchetypeConstants;

// Command fortress: chamfered walls, avenues from two gates to a ring around the keep, a function per quadrant.
void SovietPlan_CommandFortress(const FSovietPlanPen& Pen)
{
	const double Half = Pen.Size(5800.0, 6200.0);
	const double Chamfer = 1700.0;
	const double Keep = 2250.0;
	const double Inset = Half - Chamfer;

	TArray<FVector2D> KeepRing = {
		FVector2D(Keep, Keep), FVector2D(-Keep, Keep), FVector2D(-Keep, -Keep), FVector2D(Keep, -Keep), FVector2D(Keep, Keep)
	};
	Pen.Lane(KeepRing);
	Pen.Main({FVector2D(Half + RoadPastGate, 0.0), FVector2D(Keep, 0.0)});
	Pen.Main({FVector2D(-Keep, 0.0), FVector2D(-Half - RoadPastGate, 0.0)});
	Pen.Lane({FVector2D(0.0, Keep), FVector2D(0.0, Half - 500.0)});
	Pen.Lane({FVector2D(0.0, -Keep), FVector2D(0.0, -Half + 500.0)});
	Pen.Gate(FVector2D(Half, 0.0), FVector2D(1.0, 0.0), true);
	Pen.Gate(FVector2D(-Half, 0.0), FVector2D(-1.0, 0.0), true);
	Pen.FenceLoop({
		FVector2D(Half, Inset), FVector2D(Inset, Half), FVector2D(-Inset, Half), FVector2D(-Half, Inset),
		FVector2D(-Half, -Inset), FVector2D(-Inset, -Half), FVector2D(Inset, -Half), FVector2D(Half, -Inset)
	}, ESurvivalSovietWallFamily::SovietConcrete);
	Pen.Plan.bFortifyAllRuns = true;

	FSovietPlot& KeepPlot = Pen.Plot(ESurvivalSovietZone::Command, FVector2D::ZeroVector, FVector2D(1.0, 0.0),
	                                 Keep - 500.0, Keep - 500.0);
	KeepPlot.Dressing = ESovietPlotDressing::Parade;

	// Quadrants: workshop and barracks on the gate avenue, stores and guns towards the back.
	const double Flip = Pen.Chance(0.5f) ? 1.0 : -1.0;
	const double QuadX = 4000.0;
	const double QuadY = 3950.0;
	FSovietPlot& Workshop = Pen.Plot(ESurvivalSovietZone::Workshop, FVector2D(QuadX, Flip * QuadY),
	                                 FVector2D(0.0, -Flip), 1520.0, 1320.0);
	Workshop.Dressing = ESovietPlotDressing::Workshop;
	FSovietPlot& Barracks = Pen.Plot(ESurvivalSovietZone::Barracks, FVector2D(QuadX, -Flip * QuadY),
	                                 FVector2D(0.0, Flip), 1520.0, 1320.0);
	Barracks.Gap = 450.0;
	Barracks.Dressing = ESovietPlotDressing::Huts;
	FSovietPlot& Stores = Pen.Plot(ESurvivalSovietZone::Depot, FVector2D(-QuadX, Flip * QuadY),
	                               FVector2D(0.0, -Flip), 1520.0, 1320.0);
	Stores.Gap = 400.0;
	Stores.Dressing = ESovietPlotDressing::Munitions;
	FSovietPlot& Fuel = Pen.Plot(ESurvivalSovietZone::Fuel, FVector2D(-QuadX - 300.0, -Flip * (QuadY + 500.0)),
	                             FVector2D(0.0, Flip), 900.0, 700.0);
	Fuel.Gap = 300.0;

	for (const FVector2D& Pit : {
		     FVector2D(-3300.0, -Flip * 3300.0), FVector2D(-3300.0, Flip * 3300.0), FVector2D(3300.0, Flip * 3300.0)
	     })
	{
		Pen.Gun(ESurvivalSovietZone::Artillery, Pit);
	}
	for (const FVector2D& Bastion : {
		     FVector2D(Half - 700.0, Inset - 400.0), FVector2D(Half - 700.0, -Inset + 400.0),
		     FVector2D(-Half + 700.0, Inset - 400.0), FVector2D(-Half + 700.0, -Inset + 400.0)
	     })
	{
		Pen.Gun(ESurvivalSovietZone::Watch, Bastion);
	}
	const TArray<FVector2D> OuterPits = SovietPlan_Polygon(12, 7600.0, PI / 12.0);
	for (const FVector2D& Pit : OuterPits)
	{
		Pen.Gun(ESurvivalSovietZone::AntiTank, Pit);
		Pen.Gun(ESurvivalSovietZone::AirDefense, Pit);
	}
	Pen.Gun(ESurvivalSovietZone::AirDefense, FVector2D(1100.0, Keep + 1200.0));
	Pen.Gun(ESurvivalSovietZone::AirDefense, FVector2D(-1100.0, -Keep - 1200.0));
	Pen.Plan.InteriorRadius = Half - 500.0;
	Pen.Plan.OuterRadius = Half;
}

// Command Fortress adds depth: heavy bunkers on every wall, artillery in the quadrants. Needs tier 2 tech.
void FSovietPresetLibrary::AddCommandFortress(TArray<FSurvivalSovietBasePreset>& Presets) const
{
	using namespace SurvivalSovietBaseDefaults;
	FSurvivalSovietBasePreset& Preset = AddPreset(Presets, TEXT("CommandFortress"), LOCTEXT("CommandFortress", "Command Fortress"),
	                                              EArch::CommandFortress, 2, 5, 2);
	FSurvivalSovietStructureSlot Keep = Admin;
	Keep.MinStrength = 0;
	Preset.ProductionBuildings = {Keep, Factory, Barracks, Tent};
	Preset.SupportBuildings = {
		Supplies, Ammo, MakeSlot(EZone::Fuel, RusBuildings, TEXT("Rus_Building_FuelStorageTankSmall"), 0, -1, 2, 0.f), Speaker
	};
	Preset.ObstacleBelt = MakeDefaultBelt(EEnemyObstacleFormation::DoubleRow);
	Preset.ThreatPath = {
		Signature(ZisBunker, 0), Platform, PitMortar, Bunker03, PitBS3, Signature(Bunker05, 0), PitBofors, Signature(MegaBunker, 0),
		PitML20, Signature(PitB4, 0), Later(Dome, 1), PitAmpulomet, PitDShK, Hold(Guards, 8),
		Gated(Hold(MediumTanks, 4), EDiff::Hard, 0, 2), Gated(Hold(LateMediums, 6), EDiff::Brutal, 0, 2),
		Gated(Hold(HeavyTanks, 6), EDiff::Brutal, 0, 2), Gated(Hold(Breakthrough, 4), EDiff::Ironman, 0, 2),
		Gated(Patrol(MediumTanks, 3), EDiff::Hard, 0, 2), Gated(Hold(AATanks, 2), EDiff::Hard, 0, 2)
	};
	Preset.ThreatPath.Append(Reserves());
	Preset.DestroyRewards = CommandReward;
}

#undef LOCTEXT_NAMESPACE
