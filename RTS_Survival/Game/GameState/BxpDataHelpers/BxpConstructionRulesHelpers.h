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

		// --- solar  ---
		{EBuildingExpansionType::BXT_SolarLarge, WithFootprint(FreeConstructionRules, 500.f, 500.f)},
		{EBuildingExpansionType::BXT_SolarSmall, WithFootprint(FreeConstructionRules, 300.f, 300.f)},
	};
}
