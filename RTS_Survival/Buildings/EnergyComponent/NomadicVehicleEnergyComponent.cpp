// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "NomadicVehicleEnergyComponent.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

void UNomadicVehicleEnergyComponent::CacheOwnerMaterials()
{
	RTSFunctionLibrary::ReportError("Nomadic vehicle energy components do not support low energy material changes."
		"\n Component: " + GetName());
}

bool UNomadicVehicleEnergyComponent::GetShouldApplyMaterialChanges() const
{
	return false;
}
