// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyAirDropSettings.h"

#include "EnemyAirDrop.h"

UEnemyAirDropSettings::UEnemyAirDropSettings()
{
	VehicleOptions.Add(FEnemyPlacementUnit::MakeVehicle(ETankSubtype::Tank_KV3));
}

void UEnemyAirDropSettings::BuildUnits(FEnemyAirDropUnits& OutUnits) const
{
	OutUnits = FEnemyAirDropUnits();
	// The air drop adds the extras up by difficulty (Hard = base + Normal extras + Hard extras), so each list holds
	// the step from the previous difficulty's count.
	const auto AddStep = [this](TArray<FEnemyAirDropUnitEntry>& OutList, const int32 Count, const int32 PreviousCount)
	{
		const int32 Step = Count - PreviousCount;
		if (Step <= 0 || VehicleOptions.IsEmpty())
		{
			return;
		}
		FEnemyAirDropUnitEntry Entry;
		Entry.UnitOptions = VehicleOptions;
		Entry.Count = Step;
		OutList.Add(Entry);
	};
	AddStep(OutUnits.BaseUnits, CountNewToRTS, 0);
	AddStep(OutUnits.ExtraUnitsNormal, CountNormal, CountNewToRTS);
	AddStep(OutUnits.ExtraUnitsHard, CountHard, CountNormal);
	AddStep(OutUnits.ExtraUnitsBrutal, CountBrutal, CountHard);
}
