// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_Spalliners.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace SpallinersTechnologySettings
{
	constexpr float HealthMultiplier = 1.2f;
}

UTE_Spalliners::UTE_Spalliners()
	: M_HealthMultiplier(SpallinersTechnologySettings::HealthMultiplier)
{
	Technology = ETechnology::Tech_Spalliners;
	TanksToApplyTo = {
		ETankSubtype::Tank_PanzerIv,
		ETankSubtype::Tank_PzIII_J,
		ETankSubtype::Tank_PzIII_AA,
		ETankSubtype::Tank_PzIII_FLamm,
		ETankSubtype::Tank_PzIII_J_Commander,
		ETankSubtype::Tank_PzIII_M,
		ETankSubtype::Tank_PzIV_F1,
		ETankSubtype::Tank_PzIV_F1_Commander,
		ETankSubtype::Tank_PzIV_G,
		ETankSubtype::Tank_PzIV_H,
		ETankSubtype::Tank_PZIV_Rockets,
		ETankSubtype::Tank_Stug,
		ETankSubtype::Tank_PzIV_70,
		ETankSubtype::Tank_Brumbar,
	};
}

void UTE_Spalliners::ApplyOnTank_Internal(ATankMaster* Tank)
{
	if (not IsValid(Tank))
	{
		return;
	}

	UHealthComponent* HealthComponent = Tank->GetHealthComponent();
	if (not IsValid(HealthComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			Tank,
			"HealthComponent",
			"UTE_Spalliners::ApplyOnTank_Internal");
		return;
	}

	HealthComponent->SetMaxHealth(HealthComponent->GetMaxHealth() * M_HealthMultiplier);
	if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Applying spalliners health boost to " + Tank->GetName());
	}
}
