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

bool BlackboardQueries::HasAtLeastAnyXTanks(const FStrategicAIBlackboard& Blackboard, const int32 MinTanks)
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

		FoundUnits++;
		if (FoundUnits >= MinTanks)
		{
			return true;
		}
	}
	return false;
}

bool BlackboardQueries::HasAtLeastAnyXIdleUnits(const FStrategicAIBlackboard& Blackboard, const int32 MinIdleUnits)
{
	int32 FoundUnits = 0;
	for (const FBlackboardIdleUnitEntry& EachIdleUnit : Blackboard.IdleDirectControlUnits)
	{
		if (not EachIdleUnit.IdleUnit.IsValid())
		{
			continue;
		}

		FoundUnits++;
		if (FoundUnits >= MinIdleUnits)
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

bool BlackboardQueries::HasAtLeastXPlayerTanks(
	const FStrategicAIBlackboard& Blackboard,
	const int32 MinPlayerTanks)
{
	const FResultPlayerUnitCounts& PlayerUnitCounts = Blackboard.CurrentPlayerUnitCounts;
	const int32 PlayerTankCount = PlayerUnitCounts.PlayerArmoredCars
		+ PlayerUnitCounts.PlayerLightTanks
		+ PlayerUnitCounts.PlayerMediumTanks
		+ PlayerUnitCounts.PlayerHeavyTanks;
	return PlayerTankCount >= MinPlayerTanks;
}

bool BlackboardQueries::HasAtLeastXPlayerArmoredCars(
	const FStrategicAIBlackboard& Blackboard,
	const int32 MinPlayerArmoredCars)
{
	return Blackboard.CurrentPlayerUnitCounts.PlayerArmoredCars >= MinPlayerArmoredCars;
}

bool BlackboardQueries::HasAtLeastXPlayerLightTanks(
	const FStrategicAIBlackboard& Blackboard,
	const int32 MinPlayerLightTanks)
{
	return Blackboard.CurrentPlayerUnitCounts.PlayerLightTanks >= MinPlayerLightTanks;
}

bool BlackboardQueries::HasAtLeastXPlayerMediumTanks(
	const FStrategicAIBlackboard& Blackboard,
	const int32 MinPlayerMediumTanks)
{
	return Blackboard.CurrentPlayerUnitCounts.PlayerMediumTanks >= MinPlayerMediumTanks;
}

bool BlackboardQueries::HasAtLeastXPlayerHeavyTanks(
	const FStrategicAIBlackboard& Blackboard,
	const int32 MinPlayerHeavyTanks)
{
	return Blackboard.CurrentPlayerUnitCounts.PlayerHeavyTanks >= MinPlayerHeavyTanks;
}

bool BlackboardQueries::HasAtLeastXPlayerSquads(
	const FStrategicAIBlackboard& Blackboard,
	const int32 MinPlayerSquads)
{
	return Blackboard.CurrentPlayerUnitCounts.PlayerSquads >= MinPlayerSquads;
}

bool BlackboardQueries::HasAtLeastXPlayerNomadicVehicles(
	const FStrategicAIBlackboard& Blackboard,
	const int32 MinPlayerNomadicVehicles)
{
	return Blackboard.CurrentPlayerUnitCounts.PlayerNomadicVehicles >= MinPlayerNomadicVehicles;
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
	for (auto EachLoc : Blackboard.CurrentPlayerUnitCounts.PlayerResourceBuildings)
	{
		if (not EachLoc.IsNearlyZero())
		{
			return true;
		}
	}
	return false;
}

bool BlackboardQueries::HasValidPlayerHeavyTankFLankLocations(const FStrategicAIBlackboard& Blackboard)
{
	for (auto EachHeavyTankFlanking : Blackboard.AgreggatedHeavyTankFlankingResults)
	{
		for (auto EachHeavyTank : EachHeavyTankFlanking.FlankLocationsAroundHeavyTank)
		{
			if (not EachHeavyTank.Locations.IsEmpty())
			{
				return true;
			}
		}
	}
	return false;
}

TArray<FVector> BlackboardQueries::GetRandomLocationsOfIdleUnits(
	const FStrategicAIBlackboard& Blackboard,
	const int32 PreferredAmount)
{
	if (PreferredAmount <= 0)
	{
		return TArray<FVector>();
	}

	if (Blackboard.IdleDirectControlUnits.IsEmpty())
	{
		return TArray<FVector>();
	}

	const int32 MaxLocationsToPick = FMath::Min(PreferredAmount, Blackboard.IdleDirectControlUnits.Num());
	TArray<int32> RemainingIdleUnitIndices;
	RemainingIdleUnitIndices.Reserve(Blackboard.IdleDirectControlUnits.Num());

	for (int32 IdleUnitIndex = 0; IdleUnitIndex < Blackboard.IdleDirectControlUnits.Num(); ++IdleUnitIndex)
	{
		RemainingIdleUnitIndices.Add(IdleUnitIndex);
	}

	TArray<FVector> PickedLocations;
	PickedLocations.Reserve(MaxLocationsToPick);

	TSet<FVector> SeenLocations;
	SeenLocations.Reserve(MaxLocationsToPick);

	while (PickedLocations.Num() < MaxLocationsToPick && not RemainingIdleUnitIndices.IsEmpty())
	{
		const int32 RandomRemainingIndex = FMath::RandRange(0, RemainingIdleUnitIndices.Num() - 1);
		const int32 PickedIdleUnitIndex = RemainingIdleUnitIndices[RandomRemainingIndex];
		RemainingIdleUnitIndices.RemoveAtSwap(RandomRemainingIndex, 1, EAllowShrinking::No);

		const FVector IdleUnitLocation = Blackboard.IdleDirectControlUnits[PickedIdleUnitIndex].Get()->
			GetActorLocation();
		if (SeenLocations.Contains(IdleUnitLocation))
		{
			continue;
		}

		SeenLocations.Add(IdleUnitLocation);
		PickedLocations.Add(IdleUnitLocation);
	}

	return PickedLocations;
}
