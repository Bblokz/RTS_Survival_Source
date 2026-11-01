// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePlanPen.h"
#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePresetLibrary.h"

#define LOCTEXT_NAMESPACE "SurvivalSovietBaseSettings"

using namespace SovietBaseArchetypeConstants;

namespace
{
	void SovietPlan_DepotBlock(const FSovietPlanPen& Pen, const ESurvivalSovietZone Zone, const double SignX,
	                           const double SignY, const double Half)
	{
		const double Inner = 650.0;
		const double Outer = Half - 650.0;
		const double BlockHalf = (Outer - Inner) * 0.5;
		const FVector2D Center(SignX * (Inner + BlockHalf), SignY * (Inner + BlockHalf));
		FSovietPlot& Block = Pen.Plot(Zone, Center, FVector2D(0.0, -SignY), BlockHalf, BlockHalf);
		Block.Gap = 520.0;
		switch (Zone)
		{
		case ESurvivalSovietZone::Fuel:
			Block.bBerm = true;
			Block.Dressing = ESovietPlotDressing::Barrels;
			break;
		case ESurvivalSovietZone::Depot:
			Block.Dressing = ESovietPlotDressing::Crates;
			break;
		case ESurvivalSovietZone::Workshop:
			Block.Dressing = ESovietPlotDressing::Workshop;
			break;
		case ESurvivalSovietZone::Barracks:
			Block.Dressing = ESovietPlotDressing::Huts;
			break;
		default:
			Block.Dressing = ESovietPlotDressing::Munitions;
			break;
		}
	}
}

// Fortified depot: walled square, a cross of fire breaks, one purpose per block.
void SovietPlan_SupplyDepot(const FSovietPlanPen& Pen)
{
	const double Half = Pen.Size(5000.0, 5900.0);
	const bool bRearGate = Pen.Strength >= 1;
	Pen.Main({
		FVector2D(Half + RoadPastGate, 0.0),
		FVector2D(bRearGate ? -Half - RoadPastGate : -Half + 500.0, 0.0)
	});
	Pen.Lane({FVector2D(0.0, -Half + 500.0), FVector2D(0.0, Half - 500.0)});
	Pen.Gate(FVector2D(Half, 0.0), FVector2D(1.0, 0.0), true);
	if (bRearGate)
	{
		Pen.Gate(FVector2D(-Half, 0.0), FVector2D(-1.0, 0.0), true);
	}
	Pen.FenceLoop({
		FVector2D(Half, Half), FVector2D(-Half, Half), FVector2D(-Half, -Half), FVector2D(Half, -Half)
	}, ESurvivalSovietWallFamily::SovietConcrete);

	// Ammunition and fuel on opposite corners, the garrison between them.
	const double Flip = Pen.Chance(0.5f) ? 1.0 : -1.0;
	SovietPlan_DepotBlock(Pen, ESurvivalSovietZone::Fuel, -1.0, Flip, Half);
	SovietPlan_DepotBlock(Pen, ESurvivalSovietZone::Depot, 1.0, -Flip, Half);
	SovietPlan_DepotBlock(Pen, ESurvivalSovietZone::Barracks, 1.0, Flip, Half);
	SovietPlan_DepotBlock(Pen, ESurvivalSovietZone::Workshop, -1.0, -Flip, Half);

	for (const FVector2D& Corner : {
		     FVector2D(Half, Half), FVector2D(Half, -Half), FVector2D(-Half, Half), FVector2D(-Half, -Half)
	     })
	{
		Pen.Gun(ESurvivalSovietZone::Watch, Corner * 0.93);
		Pen.Gun(ESurvivalSovietZone::AirDefense, Corner * 1.12);
	}
	for (const double Sign : {1.0, -1.0})
	{
		Pen.Gun(ESurvivalSovietZone::AntiTank, FVector2D(Half + 1500.0, Sign * 2800.0));
		Pen.Gun(ESurvivalSovietZone::AntiTank, FVector2D(-Half - 1500.0, Sign * 2800.0));
		Pen.Gun(ESurvivalSovietZone::AntiTank, FVector2D(2800.0, Sign * (Half + 1500.0)));
		Pen.Gun(ESurvivalSovietZone::AntiTank, FVector2D(-2800.0, Sign * (Half + 1500.0)));
	}
	Pen.Plan.InteriorRadius = Half - 600.0;
	Pen.Plan.OuterRadius = Half;
}

// Supply Depot adds walls and storage: bunkers on the wall, stores behind it, refinery plant at the top end.
void FSovietPresetLibrary::AddSupplyDepot(TArray<FSurvivalSovietBasePreset>& Presets) const
{
	using namespace SurvivalSovietBaseDefaults;
	FSurvivalSovietBasePreset& Preset = AddPreset(Presets, TEXT("SupplyDepot"), LOCTEXT("SupplyDepot", "Supply Depot"),
	                                              EArch::FortifiedDepot, 1, 2, 1);
	Preset.ProductionBuildings = {Barracks, Tent, Factory, Cooling, Research};
	FSurvivalSovietStructureSlot MoreAmmo = Ammo;
	MoreAmmo.Count = 2;
	MoreAmmo.CountPerStrength = 1.f;
	Preset.SupportBuildings = {
		Supplies, MoreAmmo, Fuel, FuelTank,
		MakeSlot(EZone::Fuel, RusBuildings, TEXT("Rus_Zone_Refinery2"), 2, -1, 1, 0.f, EDiff::Hard)
	};
	Preset.ObstacleBelt = MakeDefaultBelt(EEnemyObstacleFormation::DoubleRow);
	Preset.ThreatPath = {
		MGBunker, Tower, PitDShK, Signature(ZisBunker, 0), Hold(Rifles, 8), AutoCn,
		PitBofors, PitSector, Dome, Bunker03, Signature(Bunker05), Signature(MegaBunker),
		Gated(Hold(TankHunters, 6), EDiff::Normal), PitMortar, Gated(Patrol(LightTanks, 3), EDiff::Normal, 0, 1),
		Gated(Hold(MediumTanks, 4), EDiff::Hard, 1, 2), Gated(Hold(HeavyTanks, 3), EDiff::Brutal, 1, 2), PitML20, PitB4,
		Gated(Hold(Breakthrough, 3), EDiff::Ironman, 2, 2)
	};
	Preset.ThreatPath.Append(Reserves());
	Preset.DestroyRewards = ArmourReward;
}

#undef LOCTEXT_NAMESPACE
