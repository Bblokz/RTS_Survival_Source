// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "EnergyComp.h"

#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


// Sets default values for this component's properties
UEnergyComp::UEnergyComp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnergyComp::SetEnabled(const bool bEnabled)
{
	bM_IsEnabled = bEnabled;

	UPlayerResourceManager* PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (PlayerResourceManager)
	{
		if (bEnabled)
		{
			PlayerResourceManager->RegisterEnergyComponent(this);
			return;
		}
		PlayerResourceManager->DeregisterEnergyComponent(this);
		return;
	}
	RTSFunctionLibrary::ReportError("Failed to get PlayerResourceManager in EnergyComp.cpp"
		"\n at function SetEnabled in EnergyComp.cpp."
		"\n for component: " + GetName());
}


void UEnergyComp::UpgradeEnergy(const int32 Amount)
{
	UpdateEnergySupplied(M_Energy + Amount);
}

void UEnergyComp::BeginPlay()
{
	Super::BeginPlay();
	if (bStartEnabled)
	{
		// Register with the player resource manager.
		SetEnabled(true);
	}
}

void UEnergyComp::BeginDestroy()
{
	if (bM_IsEnabled)
	{
		UPlayerResourceManager* PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
		if (PlayerResourceManager)
		{
			PlayerResourceManager->DeregisterEnergyComponent(this);
		}
	}
	Super::BeginDestroy();
}

void UEnergyComp::InitEnergyComponent(const int32 Energy)
{
	UpdateEnergySupplied(Energy);
}

void UEnergyComp::UpdateEnergySupplied(const int32 NewEnergySupplied)
{
	const int32 OldEnergy = M_Energy;
	M_Energy = NewEnergySupplied;
	// Only when the component is enabled, it is registered.
	if (bM_IsEnabled)
	{
		UPlayerResourceManager* PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
		if (PlayerResourceManager)
		{
			// Provide the old energy supply too so we know what amount to subtract before we add the new amount.
			PlayerResourceManager->UpdateEnergySupplyForUpgradedComponent(this, OldEnergy);
			return;
		}
		RTSFunctionLibrary::ReportError("Failed to get PlayerResourceManager in EnergyComp.cpp"
			"\n at function UpdateEnergySupplied in EnergyComp.cpp."
			"\n for component: " + GetName());
	}
}
