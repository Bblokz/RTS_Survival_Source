// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_Barricades.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace BarricadesTechnologySettings
{
	constexpr float HealthMultiplier = 1.2f;
}

UTE_Barricades::UTE_Barricades()
	: M_HealthMultiplier(BarricadesTechnologySettings::HealthMultiplier)
{
	Technology = ETechnology::Tech_Barricades;
	NomadicsToApplyTo = {
		ENomadicSubtype::Nomadic_GerHq,
		ENomadicSubtype::Nomadic_GerRefinery,
		ENomadicSubtype::Nomadic_GerMetalVault,
		ENomadicSubtype::Nomadic_GerGammaFacility,
		ENomadicSubtype::Nomadic_GerLightSteelForge,
		ENomadicSubtype::Nomadic_GerBarracks,
		ENomadicSubtype::Nomadic_GerMechanizedDepot,
		ENomadicSubtype::Nomadic_GerArmory,
		ENomadicSubtype::Nomadic_GerTrainingCentre,
		ENomadicSubtype::Nomadic_GerMedTankFactory,
		ENomadicSubtype::Nomadic_GerCommunicationCenter,
		ENomadicSubtype::Nomadic_GerAirbase,
		ENomadicSubtype::Nomadic_GerExperimentalUnitsFactory,
	};
}

void UTE_Barricades::ApplyOnNomadic_Internal(ANomadicVehicle* Nomadic)
{
	if (not IsValid(Nomadic))
	{
		return;
	}

	Nomadic->ApplyBuildingHealthMultiplier(M_HealthMultiplier);
	if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Applying barricades building health boost to " + Nomadic->GetName());
	}
}
