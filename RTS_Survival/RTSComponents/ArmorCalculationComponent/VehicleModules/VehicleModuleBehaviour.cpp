// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VehicleModuleBehaviour.h"

#include "Components/MeshComponent.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"

void UVehicleModuleBehaviour::InitializeModuleContext(const FVehicleModuleBehaviourContext& NewContext)
{
	M_ModuleContext = NewContext;
}

void UVehicleModuleBehaviour::UpdateModuleContext(const FVehicleModuleBehaviourContext& NewContext)
{
	const FVehicleModuleBehaviourContext PreviousContext = M_ModuleContext;
	M_ModuleContext = NewContext;
	OnModuleContextUpdated(PreviousContext);
}

void UVehicleModuleBehaviour::OnModuleContextUpdated(const FVehicleModuleBehaviourContext& PreviousContext)
{
	BP_OnModuleContextUpdated(M_ModuleContext);
}

UMeshComponent* UVehicleModuleBehaviour::GetBoundMesh() const
{
	return M_ModuleContext.BoundMesh.Get();
}

UArmorCalculation* UVehicleModuleBehaviour::GetModuleArmorCalculation() const
{
	return M_ModuleContext.ArmorCalculation.Get();
}
