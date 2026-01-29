// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/EnergyComponent/EnergyComp.h"

#include "BuildingExpansionEnergyComponent.generated.h"

class ABuildingExpansion;

/**
 * @brief Energy component for building expansions that defers material caching until construction completes.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UBuildingExpansionEnergyComponent : public UEnergyComp
{
	GENERATED_BODY()

protected:
	virtual void CacheOwnerMaterials() override;
	virtual bool GetShouldApplyMaterialChanges() const override;

private:
	void CacheBuildingExpansionMaterials();
	void HandleBxpConstructed();
	void HandleBxpPackingUp();
	void HandleBxpCancelledPackingUp();
	bool GetIsValidBuildingExpansion() const;

	UPROPERTY()
	TWeakObjectPtr<ABuildingExpansion> M_BuildingExpansion;

	bool bM_HasCachedMaterials = false;
	bool bM_IsPackingUp = false;
	bool bM_IsWaitingForConstruction = false;
};
