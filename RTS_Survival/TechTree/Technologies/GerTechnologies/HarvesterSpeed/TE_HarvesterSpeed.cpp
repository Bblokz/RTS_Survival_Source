// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_HarvesterSpeed.h"

#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"

namespace HarvesterSpeedTechnologySettings
{
	constexpr float HarvesterAnimationSpeedPlayRate = 2.f;
}

UTE_HarvesterSpeed::UTE_HarvesterSpeed()
	: M_HarvesterAnimationSpeedPlayRate(HarvesterSpeedTechnologySettings::HarvesterAnimationSpeedPlayRate)
{
	Technology = ETechnology::Tech_HarvesterSpeed;
	TanksToApplyTo = {
		ETankSubtype::Tank_PzI_Harvester,
	};
}

void UTE_HarvesterSpeed::ApplyOnTank_Internal(ATankMaster* Tank)
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

	Harvester->UpgradeHarvestingAnimationPlayRate(M_HarvesterAnimationSpeedPlayRate);
}
