// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_HarvesterCapacity.h"

#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"

namespace HarvesterCapacityTechnologySettings
{
	constexpr int32 RadixiteExtraCapacity = 50;
	constexpr int32 MetalExtraCapacity = 25;
}

UTE_HarvesterCapacity::UTE_HarvesterCapacity()
	: M_RadixiteExtraCapacity(HarvesterCapacityTechnologySettings::RadixiteExtraCapacity)
	  , M_MetalExtraCapacity(HarvesterCapacityTechnologySettings::MetalExtraCapacity)
{
	Technology = ETechnology::Tech_HarvesterCapacity;
	TanksToApplyTo = {
		ETankSubtype::Tank_PzI_Harvester,
	};
}

void UTE_HarvesterCapacity::ApplyOnTank_Internal(ATankMaster* Tank)
{
	if (not IsValid(Tank))
	{
		return;
	}

	UHarvester* Harvester = Tank->FindComponentByClass<UHarvester>();
	if (not IsValid(Harvester))
	{
		return;
	}

	UpgradeHarvesterCapacity(*Harvester);
}

void UTE_HarvesterCapacity::UpgradeHarvesterCapacity(UHarvester& Harvester) const
{
	Harvester.UpgradeCapacityForResource(M_RadixiteExtraCapacity, ERTSResourceType::Resource_Radixite);
	Harvester.UpgradeCapacityForResource(M_MetalExtraCapacity, ERTSResourceType::Resource_Metal);
}
