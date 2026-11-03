// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ConvoyDefinition.h"

#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyTrainingHelpers/EnemyTrainingHelpers.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"

namespace ConvoyDefinitionDefaults
{
	const TCHAR* const SovietTankerClass = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/SovietTanker/"
		"Rus_ArmoredCar_SovietTanker.Rus_ArmoredCar_SovietTanker_C");
	const TCHAR* const ZilFuelClass = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/ZilCargo/"
		"Rus_ArmoredCar_ZilFuel.Rus_ArmoredCar_ZilFuel_C");
	const TCHAR* const FuelCacheClass = TEXT(
		"/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/Radixite/BP_ScavObj_RadixiteDump.BP_ScavObj_RadixiteDump_C");

	// Cargo per escort training point: the whole convoy pays this times its guard cost.
	constexpr float FuelRadixitePerGuardPoint = 30.f;
	constexpr float FuelMetalPerGuardPoint = 10.f;
	// Free cruise on flat ground, convoy test T0 (round 1, 8 000 cm lanes on SM_Desert_01), median cm/s.
	constexpr float SovietTankerCruiseCms = 1261.f;
	constexpr float ZilFuelCruiseCms = 1006.f;

	FConvoyEscortSpec MakeEscort(const EAllUnitType UnitType, const uint8 Subtype, const int32 Count,
	                             const EConvoyStation Station)
	{
		FConvoyEscortSpec Escort;
		Escort.Unit = FTrainingOption(UnitType, Subtype);
		Escort.Count = Count;
		Escort.Station = Station;
		return Escort;
	}

	FConvoyEscortSpec MakeTankEscort(const ETankSubtype Tank, const int32 Count, const EConvoyStation Station)
	{
		return MakeEscort(EAllUnitType::UNType_Tank, static_cast<uint8>(Tank), Count, Station);
	}

	FConvoyEscortSpec MakeSquadEscort(const ESquadSubtype Squad, const int32 Count, const EConvoyStation Station)
	{
		return MakeEscort(EAllUnitType::UNType_Squad, static_cast<uint8>(Squad), Count, Station);
	}

	FConvoyTruckSpec MakeTruck(const TCHAR* ClassPath, const ETankSubtype Subtype, const int32 Count,
	                           const float CalibratedCruiseSpeed)
	{
		FConvoyTruckSpec Truck;
		Truck.TruckClass = TSoftClassPtr<AActor>(FSoftObjectPath(ClassPath));
		Truck.TrainingOption = FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(Subtype));
		Truck.Count = Count;
		Truck.CalibratedCruiseSpeedCms = CalibratedCruiseSpeed;
		return Truck;
	}
}

UConvoyDefinition::UConvoyDefinition()
{
	using namespace ConvoyDefinitionDefaults;
	// Defaults describe a fuel convoy; the vehicle-parts asset overrides trucks, cache and cargo table.
	DisplayName = NSLOCTEXT("Convoy", "DefaultConvoyName", "Fuel convoy");
	Trucks.Add(MakeTruck(SovietTankerClass, ETankSubtype::Tank_SovietTanker, 1, SovietTankerCruiseCms));
	Trucks.Add(MakeTruck(ZilFuelClass, ETankSubtype::Tank_ZilFuel, 1, ZilFuelCruiseCms));

	FConvoyGuardTier Light;
	Light.TierName = TEXT("Light");
	Light.Escorts.Add(MakeTankEscort(ETankSubtype::Tank_Ba14, 1, EConvoyStation::Lead));
	Light.Escorts.Add(MakeSquadEscort(ESquadSubtype::Squad_Rus_Mosin, 1, EConvoyStation::LeftFlank));
	Light.Escorts.Add(MakeSquadEscort(ESquadSubtype::Squad_Rus_Mosin, 1, EConvoyStation::RightFlank));
	GuardTiers.Add(Light);

	FConvoyGuardTier Medium;
	Medium.TierName = TEXT("Medium");
	Medium.ExtraTrucks = 1;
	Medium.Escorts.Add(MakeTankEscort(ETankSubtype::Tank_BT7, 1, EConvoyStation::Lead));
	Medium.Escorts.Add(MakeTankEscort(ETankSubtype::Tank_Ba14, 1, EConvoyStation::Rear));
	Medium.Escorts.Add(MakeSquadEscort(ESquadSubtype::Squad_Rus_Mosin, 1, EConvoyStation::LeftFlank));
	Medium.Escorts.Add(MakeSquadEscort(ESquadSubtype::Squad_Rus_Mosin, 1, EConvoyStation::RightFlank));
	Medium.Escorts.Add(MakeSquadEscort(ESquadSubtype::Squad_Rus_Okhotnik, 1, EConvoyStation::Rear));
	GuardTiers.Add(Medium);

	FConvoyGuardTier Heavy;
	Heavy.TierName = TEXT("Heavy");
	Heavy.ExtraTrucks = 2;
	// Only vehicles faster than the truck pace (T-34s and KVs top out below it: 26 / 12 km/h).
	Heavy.Escorts.Add(MakeTankEscort(ETankSubtype::Tank_T70F, 1, EConvoyStation::Lead));
	Heavy.Escorts.Add(MakeTankEscort(ETankSubtype::Tank_Ba12, 1, EConvoyStation::LeftFlank));
	Heavy.Escorts.Add(MakeTankEscort(ETankSubtype::Tank_T70, 1, EConvoyStation::RightFlank));
	Heavy.Escorts.Add(MakeTankEscort(ETankSubtype::Tank_BT7_4, 1, EConvoyStation::Rear));
	Heavy.Escorts.Add(MakeSquadEscort(ESquadSubtype::Squad_Rus_Mosin, 1, EConvoyStation::LeftFlank));
	Heavy.Escorts.Add(MakeSquadEscort(ESquadSubtype::Squad_Rus_Mosin, 1, EConvoyStation::RightFlank));
	Heavy.Escorts.Add(MakeSquadEscort(ESquadSubtype::Squad_Rus_Okhotnik, 1, EConvoyStation::RightFlank));
	Heavy.Escorts.Add(MakeSquadEscort(ESquadSubtype::Squad_Rus_Dyakonov, 1, EConvoyStation::Rear));
	GuardTiers.Add(Heavy);

	Cargo.CargoType = EConvoyCargoType::Fuel;
	Cargo.CacheClass = TSoftClassPtr<AScavengeableObject>(FSoftObjectPath(FuelCacheClass));
	Cargo.ResourcesPerGuardPoint.Add(ERTSResourceType::Resource_Radixite, FuelRadixitePerGuardPoint);
	Cargo.ResourcesPerGuardPoint.Add(ERTSResourceType::Resource_Metal, FuelMetalPerGuardPoint);
}

const FConvoyGuardTier* UConvoyDefinition::GetGuardTier(const int32 Index) const
{
	if (GuardTiers.IsEmpty())
	{
		return nullptr;
	}
	return &GuardTiers[FMath::Clamp(Index, 0, GuardTiers.Num() - 1)];
}

TArray<FConvoyTruckSpec> UConvoyDefinition::GetTruckColumn(const int32 TierIndex) const
{
	TArray<FConvoyTruckSpec> Column;
	for (const FConvoyTruckSpec& Truck : Trucks)
	{
		for (int32 Copy = 0; Copy < Truck.Count; ++Copy)
		{
			FConvoyTruckSpec Single = Truck;
			Single.Count = 1;
			Column.Add(Single);
		}
	}
	const FConvoyGuardTier* Tier = GetGuardTier(TierIndex);
	if (Tier == nullptr or Column.IsEmpty())
	{
		return Column;
	}
	// Extra trucks cycle through the authored trucks so a heavier tier keeps the cargo mix.
	const int32 AuthoredCount = Column.Num();
	for (int32 Extra = 0; Extra < Tier->ExtraTrucks; ++Extra)
	{
		// Copy first: adding an element of the same array may reallocate it under the reference.
		const FConvoyTruckSpec ExtraTruck = Column[Extra % AuthoredCount];
		Column.Add(ExtraTruck);
	}
	return Column;
}

int32 UConvoyDefinition::GetGuardTrainingPoints(const int32 TierIndex) const
{
	const FConvoyGuardTier* Tier = GetGuardTier(TierIndex);
	if (Tier == nullptr)
	{
		return 0;
	}
	int32 Points = 0;
	for (const FConvoyEscortSpec& Escort : Tier->Escorts)
	{
		int32 UnitPoints = 0;
		if (Escort.Unit.UnitType == EAllUnitType::UNType_Tank)
		{
			UnitPoints = EnemyTrainingHelpers::GetTankTrainingPointsCost(static_cast<ETankSubtype>(Escort.Unit.SubtypeValue));
		}
		else if (Escort.Unit.UnitType == EAllUnitType::UNType_Squad)
		{
			UnitPoints = EnemyTrainingHelpers::GetSquadTrainingPointCost(static_cast<ESquadSubtype>(Escort.Unit.SubtypeValue));
		}
		Points += UnitPoints * Escort.Count;
	}
	return Points;
}

FConvoyCargoValue UConvoyDefinition::GetTotalCargo(const int32 TierIndex, const float CargoMultiplier) const
{
	FConvoyCargoValue Total;
	const float GuardPoints = static_cast<float>(GetGuardTrainingPoints(TierIndex));
	for (const TPair<ERTSResourceType, float>& Rate : Cargo.ResourcesPerGuardPoint)
	{
		const int32 Amount = FMath::RoundToInt(GuardPoints * Rate.Value * FMath::Max(0.f, CargoMultiplier));
		if (Amount > 0)
		{
			Total.Resources.Add(Rate.Key, Amount);
		}
	}
	return Total;
}

FConvoyCargoValue UConvoyDefinition::GetCargoPerTruck(const int32 TierIndex, const float CargoMultiplier) const
{
	const FConvoyCargoValue Total = GetTotalCargo(TierIndex, CargoMultiplier);
	const int32 TruckCount = FMath::Max(1, GetTruckColumn(TierIndex).Num());
	FConvoyCargoValue PerTruck;
	for (const TPair<ERTSResourceType, int32>& Pair : Total.Resources)
	{
		PerTruck.Resources.Add(Pair.Key, FMath::DivideAndRoundUp(Pair.Value, TruckCount));
	}
	return PerTruck;
}

TArray<FString> UConvoyDefinition::Validate() const
{
	TArray<FString> Problems;
	if (Trucks.IsEmpty())
	{
		Problems.Add(TEXT("no trucks"));
	}
	for (const FConvoyTruckSpec& Truck : Trucks)
	{
		if (Truck.TruckClass.IsNull())
		{
			Problems.Add(TEXT("a truck entry has no class"));
		}
	}
	if (GuardTiers.IsEmpty())
	{
		Problems.Add(TEXT("no guard tiers"));
	}
	if (Cargo.CacheClass.IsNull())
	{
		Problems.Add(TEXT("no cargo cache class"));
	}
	return Problems;
}
