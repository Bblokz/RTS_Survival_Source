// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePlanPen.h"
#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePresetLibrary.h"

#define LOCTEXT_NAMESPACE "SurvivalSovietBaseSettings"

using namespace SovietBaseArchetypeConstants;

namespace
{
	void SovietPlan_HillTrack(const FSovietPlanPen& Pen, const FVector2D& Target, const FSurvivalSovietSiteMap* Site,
	                          const TFunction<FVector2D(const FVector2D&)>& ToLocal,
	                          const TFunction<FVector2D(const FVector2D&)>& ToWorld)
	{
		TArray<FVector2D> Points;
		if (Site != nullptr)
		{
			const TArray<FVector2D> WorldPath = Site->FindDrivablePath(ToWorld(FVector2D::ZeroVector), ToWorld(Target));
			for (const FVector2D& Point : WorldPath)
			{
				Points.Add(ToLocal(Point));
			}
		}
		if (Points.Num() < 2)
		{
			// No drivable route to this crest (or no terrain data): the track ends on the flat.
			const double Reach = FMath::Min(Target.Size(), 6300.0);
			Points = {FVector2D::ZeroVector, Target.GetSafeNormal() * Reach};
		}
		Pen.Track(Points);
	}

	void SovietPlan_HillTrenches(const FSovietPlanPen& Pen, const FSurvivalSovietSiteMap* Site,
	                             const TFunction<FVector2D(const FVector2D&)>& ToWorld)
	{
		constexpr int32 ArcCount = 10;
		constexpr double ArcRadius = 6900.0;
		constexpr double FillShare = 0.72;
		// No closed wall here: this circle is what counts as the outside of the base.
		constexpr int32 PerimeterLineSides = 20;
		Pen.Plan.WallFamily = ESurvivalSovietWallFamily::Sandbag;
		Pen.Plan.PerimeterLine = SovietPlan_Polygon(PerimeterLineSides, ArcRadius - 280.0, 0.0);
		const double Phase = Pen.Random.FRandRange(0.0, 2.0 * PI / ArcCount);
		for (int32 Arc = 0; Arc < ArcCount; ++Arc)
		{
			const double Middle = Phase + 2.0 * PI * Arc / ArcCount;
			const FVector2D MiddlePoint = SovietPlan_Polar(ArcRadius, Middle);
			if (Site != nullptr)
			{
				const FVector2D WorldMiddle = ToWorld(MiddlePoint);
				const FVector2D WorldCenter = ToWorld(FVector2D::ZeroVector);
				const double WorldYaw = FMath::Atan2(WorldMiddle.Y - WorldCenter.Y, WorldMiddle.X - WorldCenter.X);
				if (Site->GetApproachOpenness(WorldYaw) < 0.5)
				{
					continue;
				}
			}
			// A trench zig-zags: every other arc sits further out.
			const double Radius = ArcRadius + (Arc % 2 == 0 ? 280.0 : -280.0);
			const double HalfSpan = PI / ArcCount * FillShare;
			FSovietFenceRun& Run = Pen.Plan.Fence.AddDefaulted_GetRef();
			Run.Start = SovietPlan_Polar(Radius, Middle - HalfSpan);
			Run.End = SovietPlan_Polar(Radius, Middle + HalfSpan);
		}
	}
}

// Hill stronghold: tracks from a tight core out to the crests; trench arcs across the open approaches only.
void SovietPlan_HillStronghold(
	const FSovietPlanPen& Pen,
	const FSurvivalSovietSiteMap* Site,
	const TFunction<FVector2D(const FVector2D&)>& ToLocal,
	const TFunction<FVector2D(const FVector2D&)>& ToWorld,
	const TArray<FVector2D>& DefensePointsLocal)
{
	// The road comes in with a bend, so the gate has no straight line of fire to the core.
	const double Bend = Pen.Chance(0.5f) ? 1500.0 : -1500.0;
	Pen.Main({
		FVector2D(7000.0 + RoadPastGate, 0.0), FVector2D(5000.0, 0.0), FVector2D(3400.0, Bend),
		FVector2D(1400.0, Bend), FVector2D(0.0, 0.0)
	});
	Pen.Gate(FVector2D(7000.0, 0.0), FVector2D(1.0, 0.0), false);

	// Up to three tracks to the crests; without crests, tracks to the other approaches.
	TArray<FVector2D> Targets;
	for (const FVector2D& Point : DefensePointsLocal)
	{
		const bool bFarFromOthers = not Targets.ContainsByPredicate([&Point](const FVector2D& Other)
		{
			return FVector2D::DotProduct(Other.GetSafeNormal(), Point.GetSafeNormal()) > 0.5;
		});
		if (bFarFromOthers and Targets.Num() < 3 and Point.X < 3000.0)
		{
			Targets.Add(Point);
		}
	}
	if (Targets.Num() < 2)
	{
		Targets = {SovietPlan_Polar(6300.0, 2.1), SovietPlan_Polar(6300.0, -2.1)};
	}
	for (const FVector2D& Target : Targets)
	{
		SovietPlan_HillTrack(Pen, Target, Site, ToLocal, ToWorld);
		const FVector2D Direction = Target.GetSafeNormal();
		const FVector2D Side(-Direction.Y, Direction.X);
		Pen.Gun(ESurvivalSovietZone::AntiTank, Direction * 4700.0 + Side * 1300.0);
		Pen.Gun(ESurvivalSovietZone::AntiTank, Direction * 4700.0 - Side * 1300.0);
	}

	// Tight core: everything hugs the central square.
	const ESurvivalSovietZone CoreZones[] = {
		ESurvivalSovietZone::Command, ESurvivalSovietZone::Workshop, ESurvivalSovietZone::Barracks,
		ESurvivalSovietZone::Depot
	};
	const double FirstYaw = Pen.Random.FRandRange(0.5, 0.9);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector2D Direction = SovietPlan_Polar(1.0, FirstYaw + Index * HALF_PI);
		FSovietPlot& Core = Pen.Plot(CoreZones[Index], Direction * 2500.0, -Direction, 1800.0, 1600.0);
		Core.Gap = 480.0;
		Core.Dressing = Index == 3 ? ESovietPlotDressing::Crates : ESovietPlotDressing::None;
	}
	FSovietPlot& Fuel = Pen.Plot(ESurvivalSovietZone::Fuel, SovietPlan_Polar(4700.0, FirstYaw + PI), SovietPlan_Polar(1.0, FirstYaw),
	                             1200.0, 750.0);
	Fuel.bBerm = true;
	Fuel.Gap = 320.0;

	for (const FVector2D& Pit : SovietPlan_Polygon(8, 5500.0, Pen.Random.FRandRange(0.0, 0.7)))
	{
		Pen.Gun(ESurvivalSovietZone::Artillery, Pit * 0.72);
		Pen.Gun(ESurvivalSovietZone::AntiTank, Pit);
		Pen.Gun(ESurvivalSovietZone::AirDefense, Pit);
		Pen.Gun(ESurvivalSovietZone::Watch, Pit * 1.12);
	}
	SovietPlan_HillTrenches(Pen, Site, ToWorld);
	Pen.Plan.RidgeGunShare = 1.0;
	Pen.Plan.InteriorRadius = 6200.0;
	Pen.Plan.OuterRadius = 7200.0;
	Pen.Plan.bFortifyAllRuns = true;
}

// Hill Stronghold adds strongpoints: more crests held, more guns dug in, a small mobile force.
void FSovietPresetLibrary::AddHillStronghold(TArray<FSurvivalSovietBasePreset>& Presets) const
{
	using namespace SurvivalSovietBaseDefaults;
	FSurvivalSovietBasePreset& Preset = AddPreset(Presets, TEXT("HillStronghold"), LOCTEXT("HillStronghold", "Hill Stronghold"),
	                                              EArch::HillStronghold, 1, 5, 0);
	FSurvivalSovietStructureSlot SmallFuel = MakeSlot(EZone::Fuel, RusBuildings, TEXT("Rus_Building_FuelStorageTankSmall"), 0, -1, 2, 0.f);
	Preset.ProductionBuildings = {Admin, Barracks, Tent, Cooling};
	Preset.SupportBuildings = {Ammo, SmallFuel, Speaker};
	Preset.ObstacleBelt = MakeDefaultBelt(EEnemyObstacleFormation::Arc);
	Preset.ObstacleBelt.ArcSpanDegrees = 40.f;
	Preset.ThreatPath = {
		Pit53K, PitMortar, MGBunker, Hold(Rifles, 8), PitDShK, LongCamo,
		Signature(PitBS3), PitAmpulomet, Dome, Later(Tower, 1), PitBofors, Bunker03, Signature(PitML20), Signature(PitB4), Bunker05,
		Gated(Hold(TankHunters, 6), EDiff::Normal), Gated(Patrol(LightTanks, 2), EDiff::Normal, 0, 1),
		Gated(Patrol(MediumTanks, 2), EDiff::Hard, 1, 2), Gated(Hold(Guards, 8), EDiff::Hard, 1),
		Gated(Hold(HeavyTanks, 4), EDiff::Brutal, 1, 2), Gated(Hold(Breakthrough, 3), EDiff::Ironman, 2, 2)
	};
	Preset.ThreatPath.Append(Reserves());
	Preset.DestroyRewards = CommandReward;
}

#undef LOCTEXT_NAMESPACE
