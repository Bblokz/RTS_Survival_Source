// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "BuildingExpansionEnergyComponent.h"

#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

void UBuildingExpansionEnergyComponent::CacheOwnerMaterials()
{
	ABuildingExpansion* BuildingExpansion = Cast<ABuildingExpansion>(GetOwner());
	if (BuildingExpansion == nullptr)
	{
		return;
	}

	M_BuildingExpansion = BuildingExpansion;

	const TWeakObjectPtr<UBuildingExpansionEnergyComponent> WeakThis = this;
	BuildingExpansion->OnBxpConstructed.AddLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		UBuildingExpansionEnergyComponent* StrongThis = WeakThis.Get();
		StrongThis->HandleBxpConstructed();
	});
	BuildingExpansion->OnBxpPackingUp.AddLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		UBuildingExpansionEnergyComponent* StrongThis = WeakThis.Get();
		StrongThis->HandleBxpPackingUp();
	});
	BuildingExpansion->OnBxpCancelPackingUp.AddLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		UBuildingExpansionEnergyComponent* StrongThis = WeakThis.Get();
		StrongThis->HandleBxpCancelledPackingUp();
	});

	if (BuildingExpansion->GetStatus() == EBuildingExpansionStatus::BXS_Built)
	{
		CacheBuildingExpansionMaterials();
		return;
	}

	bM_IsWaitingForConstruction = true;
}

bool UBuildingExpansionEnergyComponent::GetShouldApplyMaterialChanges() const
{
	return bM_HasCachedMaterials && not bM_IsWaitingForConstruction && not bM_IsPackingUp;
}

void UBuildingExpansionEnergyComponent::CacheBuildingExpansionMaterials()
{
	if (bM_HasCachedMaterials)
	{
		return;
	}

	if (not GetIsValidBuildingExpansion())
	{
		return;
	}

	CacheMeshesFromActor(M_BuildingExpansion.Get());

	const TArray<ACPPTurretsMaster*> Turrets = M_BuildingExpansion->GetTurrets();
	for (ACPPTurretsMaster* Turret : Turrets)
	{
		CacheMeshesFromActor(Turret);
	}

	bM_HasCachedMaterials = true;
	bM_IsWaitingForConstruction = false;

	if (GetDoesStrategyUseMaterials() && GetShouldApplyMaterialChanges())
	{
		ApplyMaterialsForCurrentEnergyState();
	}
}

void UBuildingExpansionEnergyComponent::HandleBxpConstructed()
{
	if (GetIsShuttingDown())
	{
		return;
	}

	CacheBuildingExpansionMaterials();
}

void UBuildingExpansionEnergyComponent::HandleBxpPackingUp()
{
	if (GetIsShuttingDown())
	{
		return;
	}

	bM_IsPackingUp = true;
}

void UBuildingExpansionEnergyComponent::HandleBxpCancelledPackingUp()
{
	if (GetIsShuttingDown())
	{
		return;
	}

	bM_IsPackingUp = false;

	if (GetDoesStrategyUseMaterials() && bM_HasCachedMaterials)
	{
		ApplyMaterialsForCurrentEnergyState();
	}
}

bool UBuildingExpansionEnergyComponent::GetIsValidBuildingExpansion() const
{
	if (not M_BuildingExpansion.IsValid())
	{
		if (GetIsShuttingDown())
		{
			return false;
		}

		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_BuildingExpansion"),
			TEXT("GetIsValidBuildingExpansion"),
			this
		);

		return false;
	}

	return true;
}
