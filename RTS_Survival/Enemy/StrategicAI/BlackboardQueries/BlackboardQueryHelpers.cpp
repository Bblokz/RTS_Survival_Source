#include "BlackboardQueryHelpers.h"

#include "RTS_Survival/Enemy/StrategicAI/StrategicAIBlackboard.h"

namespace
{
bool HasAtLeastXUnitsOfType(
	const FStrategicAIBlackboard* const Blackboard,
	const int32 MinUnits,
	const EAllUnitType UnitType)
{
	if (not Blackboard)
	{
		return false;
	}

	int32 FoundUnits = 0;
	for (const FBlackboardIdleUnitEntry& EachIdleUnit : Blackboard->IdleDirectControlUnits)
	{
		if (not EachIdleUnit.IdleUnit.IsValid())
		{
			continue;
		}

		if (EachIdleUnit.UnitType != UnitType)
		{
			continue;
		}

		FoundUnits++;
		if (FoundUnits >= MinUnits)
		{
			return true;
		}
	}

	return false;
}

using FTankSubtypeMatcher = bool (*)(ETankSubtype);

bool HasAtLeastXTanksMatchingSubtypeCategory(
	const FStrategicAIBlackboard& Blackboard,
	const int32 MinTanks,
	const FTankSubtypeMatcher GetIsMatchingSubtype)
{
	int32 FoundUnits = 0;
	for (const FBlackboardIdleUnitEntry& EachIdleUnit : Blackboard.IdleDirectControlUnits)
	{
		if (not EachIdleUnit.IdleUnit.IsValid())
		{
			continue;
		}

		if (EachIdleUnit.UnitType != EAllUnitType::UNType_Tank)
		{
			continue;
		}

		const ETankSubtype TankSubtype = static_cast<ETankSubtype>(EachIdleUnit.UnitSubtypeRaw);
		if (not GetIsMatchingSubtype(TankSubtype))
		{
			continue;
		}

		FoundUnits++;
		if (FoundUnits >= MinTanks)
		{
			return true;
		}
	}

	return false;
}
}

bool BlackboardQueries::HasAtLeastXTanks(const FStrategicAIBlackboard& Blackboard, const int32 MinTanks,
                                         const ETankSubtype TankType)
{
	int32 FoundUnits = 0;
	for (const FBlackboardIdleUnitEntry& EachIdleUnit : Blackboard.IdleDirectControlUnits)
	{
		if (not EachIdleUnit.IdleUnit.IsValid())
		{
			continue;
		}

		if (EachIdleUnit.UnitType != EAllUnitType::UNType_Tank)
		{
			continue;
		}

		if (EachIdleUnit.UnitSubtypeRaw != static_cast<int32>(TankType))
		{
			continue;
		}

		FoundUnits++;
		if (FoundUnits >= MinTanks)
		{
			return true;
		}
	}

	return false;
}

bool BlackboardQueries::HasAtLeastXArmoredCars(
	const FStrategicAIBlackboard& Blackboard,
	const int32 MinArmoredCars)
{
	return HasAtLeastXTanksMatchingSubtypeCategory(Blackboard, MinArmoredCars, Global_GetIsArmoredCar);
}

bool BlackboardQueries::HasAtLeastXLightTanks(
	const FStrategicAIBlackboard& Blackboard,
	const int32 MinLightTanks)
{
	return HasAtLeastXTanksMatchingSubtypeCategory(Blackboard, MinLightTanks, Global_GetIsLightTank);
}

bool BlackboardQueries::HasAtLeastXMediumTanks(
	const FStrategicAIBlackboard& Blackboard,
	const int32 MinMediumTanks)
{
	return HasAtLeastXTanksMatchingSubtypeCategory(Blackboard, MinMediumTanks, Global_GetIsMediumTank);
}

bool BlackboardQueries::HasAtLeastXHeavyTanks(
	const FStrategicAIBlackboard& Blackboard,
	const int32 MinHeavyTanks)
{
	return HasAtLeastXTanksMatchingSubtypeCategory(Blackboard, MinHeavyTanks, Global_GetIsHeavyTank);
}

bool BlackboardQueries::HasAtLeastXAircraft(const FStrategicAIBlackboard& Blackboard, const int32 MinAircraft,
                                            EAircraftSubtype AircraftType)
{

	int32 FoundUnits = 0;
	for (const FBlackboardIdleUnitEntry& EachIdleUnit : Blackboard.IdleDirectControlUnits)
	{
		if (not EachIdleUnit.IdleUnit.IsValid())
		{
			continue;
		}

		if (EachIdleUnit.UnitType != EAllUnitType::UNType_Aircraft)
		{
			continue;
		}

		if (EachIdleUnit.UnitSubtypeRaw != static_cast<int32>(AircraftType))
		{
			continue;
		}

		FoundUnits++;
		if (FoundUnits >= MinAircraft)
		{
			return true;
		}
	}

	return false;
}

bool BlackboardQueries::HasAtLeastXSquads(const FStrategicAIBlackboard& Blackboard, const int32 MinSquads,
                                          ESquadSubtype SquadType) 
{

	int32 FoundUnits = 0;
	for (const FBlackboardIdleUnitEntry& EachIdleUnit : Blackboard.IdleDirectControlUnits)
	{
		if (not EachIdleUnit.IdleUnit.IsValid())
		{
			continue;
		}

		if (EachIdleUnit.UnitType != EAllUnitType::UNType_Squad)
		{
			continue;
		}

		if (EachIdleUnit.UnitSubtypeRaw != static_cast<int32>(SquadType))
		{
			continue;
		}

		FoundUnits++;
		if (FoundUnits >= MinSquads)
		{
			return true;
		}
	}

	return false;
}

bool BlackboardQueries::HasValidPlayerHQLocation(const FStrategicAIBlackboard& Blackboard)
{
	return not Blackboard.CurrentPlayerUnitCounts.PlayerHQLocation.IsNearlyZero();
}

bool BlackboardQueries::HasValidPlayerResourceBuildingLocations(const FStrategicAIBlackboard& Blackboard)
{
	for(auto EachLoc : Blackboard.CurrentPlayerUnitCounts.PlayerResourceBuildings)
	{
		if (not EachLoc.IsNearlyZero())
		{
			return true;
		}
	}
	return false;
}
