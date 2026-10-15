#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"

#include "RTS_Survival/Buildings/BuildingExpansion/BXPConstructionRules/BXPConstructionRules.h"

namespace BxpConstructionRules
{
	// helper: return copy of rules with a footprint
	static FORCEINLINE FBxpConstructionRules WithFootprint(const FBxpConstructionRules& Base, float SizeX, float SizeY)
	{
		FBxpConstructionRules Out = Base;
		Out.Footprint.SizeX = SizeX;
		Out.Footprint.SizeY = SizeY;
		return Out;
	}

	static const FBxpConstructionRules FreeConstructionRules = FBxpConstructionRules(
		EBxpConstructionType::Free,
		true,
		true,
		NAME_None);
	static const FBxpConstructionRules SocketConstructionRules = FBxpConstructionRules(
		EBxpConstructionType::Socket,
		false,
		true,
		NAME_None);
	static const FBxpConstructionRules OriginConstructionRules = FBxpConstructionRules(
		EBxpConstructionType::AtBuildingOrigin,
		false,
		true,
		NAME_None);

	// Defines the Init All Game ConstructionRules data use for nomadic vehicles that can expand with Bxps.
	static const TMap<EBuildingExpansionType, FBxpConstructionRules> TGameBxpConstructionRules = {
		// --- German HQ examples ---
		{EBuildingExpansionType::BTX_GerHQRadar, WithFootprint(SocketConstructionRules, 500.f, 500.f)},
		{EBuildingExpansionType::BTX_GerHQPlatform, WithFootprint(SocketConstructionRules, 500.f, 500.f)},
		{EBuildingExpansionType::BTX_GerHQHarvester, WithFootprint(SocketConstructionRules, 500.f, 500.f)},
		{EBuildingExpansionType::BTX_GerHQRepairBay, WithFootprint(FreeConstructionRules, 700.f, 350.f)},
		{EBuildingExpansionType::BTX_GerHQTower, WithFootprint(OriginConstructionRules, 200.f, 200.f)},

		// Refinery Expansions.
		{EBuildingExpansionType::BTX_RefConverter, WithFootprint(SocketConstructionRules, 800.f, 400.f)},

		// Ger Light Factory Expansions.
		{EBuildingExpansionType::BTX_GerLFactoryRadixiteReactor, WithFootprint(OriginConstructionRules, 200.f, 200.f)},
		{EBuildingExpansionType::BTX_GerLFactoryFuelStorage, WithFootprint(OriginConstructionRules, 200.f, 200.f)},

		// Ger Barracks Expansions.
		{EBuildingExpansionType::BTX_GerBarracksRadixReactor, WithFootprint(OriginConstructionRules, 500.f, 300.f)},
		{EBuildingExpansionType::BTX_GerBarrackFuelCell, WithFootprint(OriginConstructionRules, 500.f, 300.f)},

		// --- Free-placed defenses ---
		// Note that the default construction grid square is 200 in size.
		{EBuildingExpansionType::BTX_37mmFlak, WithFootprint(FreeConstructionRules, 600.f, 600.f)},
		{EBuildingExpansionType::BTX_20mmFlak, WithFootprint(FreeConstructionRules, 300.f, 300.f)},
		{EBuildingExpansionType::BXT_88mmFlak, WithFootprint(FreeConstructionRules, 600.f, 600.f)},
		{EBuildingExpansionType::BTX_Pak38, WithFootprint(FreeConstructionRules, 500.f, 500.f)},
		{EBuildingExpansionType::BTX_LeFH_150mm, WithFootprint(FreeConstructionRules, 800.f, 800.f)},
		// Turntable gun pit: the B-4 trail sweeps a 6 m circle inside a 20 m earth berm.
		{EBuildingExpansionType::BTX_RusB4HowitzerPit, WithFootprint(FreeConstructionRules, 2000.f, 2000.f)},
		{EBuildingExpansionType::BTX_GerCapturedB4Pit, WithFootprint(FreeConstructionRules, 2000.f, 2000.f)},
		{EBuildingExpansionType::BTX_RusTurretPillbox, WithFootprint(FreeConstructionRules, 900.f, 900.f)},
		{EBuildingExpansionType::BTX_GerPak40Pit, WithFootprint(FreeConstructionRules, 1000.f, 1000.f)},
		{EBuildingExpansionType::BTX_GerLeFH18Pit, WithFootprint(FreeConstructionRules, 1400.f, 1400.f)},
		{EBuildingExpansionType::BTX_GerSFH18Pit, WithFootprint(FreeConstructionRules, 1400.f, 1400.f)},
		// Small round MG pit: 2.8 m inside the bags, the earth berm runs out to about 7 m across.
		{EBuildingExpansionType::BTX_RusDShKPit, WithFootprint(FreeConstructionRules, 800.f, 800.f)},
		// Small round Ampulomet pit, a little wider than the MG pit for the gun table and the ampoule crates.
		{EBuildingExpansionType::BTX_RusAmpulometPit, WithFootprint(FreeConstructionRules, 900.f, 900.f)},
		// The 53-K and its trails inside a bag ring several bags thick.
		{EBuildingExpansionType::BTX_Rus53KPit, WithFootprint(FreeConstructionRules, 1150.f, 1150.f)},
		// The same gun held to a 90 degree arc: only that wedge is swept, so a small square wall holds it.
		{EBuildingExpansionType::BTX_Rus53KSectorPit, WithFootprint(FreeConstructionRules, 920.f, 920.f)},
		// Same emplacement as the B-4 pit.
		{EBuildingExpansionType::BTX_RusML20Pit, WithFootprint(FreeConstructionRules, 2000.f, 2000.f)},
		// The BS-3 turns a full circle with its wide trails inside a square bag wall.
		{EBuildingExpansionType::BTX_RusBS3Pit, WithFootprint(FreeConstructionRules, 1300.f, 1300.f)},
		// The Flak 37 and its crew turn inside a ring like the Pak pit's, a little wider.
		{EBuildingExpansionType::BTX_GerFlak37Pit, WithFootprint(FreeConstructionRules, 1100.f, 1100.f)},
		{EBuildingExpansionType::BTX_GerMarderStugFactory, WithFootprint(FreeConstructionRules, 800.f, 800.f)},

		// --- solar  ---
		{EBuildingExpansionType::BXT_SolarLarge, WithFootprint(FreeConstructionRules, 500.f, 500.f)},
		{EBuildingExpansionType::BXT_SolarSmall, WithFootprint(FreeConstructionRules, 300.f, 300.f)},
	};
}
