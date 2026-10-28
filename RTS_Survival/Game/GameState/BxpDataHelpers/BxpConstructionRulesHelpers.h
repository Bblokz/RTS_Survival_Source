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

		// Communication Center Expansions.
		// The observation post in socket space: 3.75 m out of the sandbag-wing end wall, 5.17 m along it.
		{EBuildingExpansionType::BTX_GerCommObservationPost, WithFootprint(SocketConstructionRules, 375.f, 517.f)},
		// Both nests sit on the wing ends of the building itself; each deck is a 4.3 m square.
		{EBuildingExpansionType::BTX_GerCommMG42Nests, WithFootprint(OriginConstructionRules, 430.f, 430.f)},

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
		// Small field defenses: the Puppchen's 4 m bag ring with its ammo boxes, the Panzerschreck's 3.3 m foxhole.
		{EBuildingExpansionType::BTX_GerPuppchenPit, WithFootprint(FreeConstructionRules, 500.f, 500.f)},
		{EBuildingExpansionType::BTX_GerPanzerschreckPit, WithFootprint(FreeConstructionRules, 400.f, 400.f)},
		// Two four-metre-spaced MG rings plus their shared ammunition corridor fill an approximately 8 m square.
		{EBuildingExpansionType::BTX_GerTwinMG42Pit, WithFootprint(FreeConstructionRules, 900.f, 900.f)},
		// The concrete block is 10.8 m across at its foot; the props and the rear ladder stand just outside it.
		{EBuildingExpansionType::BTX_GerRailPakBunker, WithFootprint(FreeConstructionRules, 1250.f, 1250.f)},
		// The square bag walls are 9.5 m across; the ammo niche bulges 1.2 m out of the left wall.
		{EBuildingExpansionType::BTX_GerFlak88Pit, WithFootprint(FreeConstructionRules, 1200.f, 1200.f)},
		// Compact square mortar pits whose walls are approximately 9 m across.
		{EBuildingExpansionType::BTX_GerMortarPit, WithFootprint(FreeConstructionRules, 1000.f, 1000.f)},
		{EBuildingExpansionType::BTX_Rus120MortarPit, WithFootprint(FreeConstructionRules, 1000.f, 1000.f)},
		{EBuildingExpansionType::BTX_GerMarderStugFactory, WithFootprint(FreeConstructionRules, 800.f, 800.f)},
		// The assembly yard platform with its exit ramp and the casemate's glacis: 43.9 x 32.4 m, its origin 1.65 m
		// behind the middle, so the footprint is sized to twice the far half (23.6 m forward).
		{EBuildingExpansionType::BTX_RusAssemblyYard, WithFootprint(FreeConstructionRules, 4800.f, 3300.f)},

		// --- solar  ---
		// The radixite solar trailer snaps to the Gamma Facility's BXP_L / BXP_R side sockets (4.5 m out, 4 m along the wall).
		{EBuildingExpansionType::BXT_SolarLarge, WithFootprint(SocketConstructionRules, 450.f, 500.f)},
		{EBuildingExpansionType::BXT_SolarSmall, WithFootprint(FreeConstructionRules, 300.f, 300.f)},
	};
}
