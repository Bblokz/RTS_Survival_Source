// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_HarvesterCapacity.generated.h"

class UHarvester;
class ATankMaster;

/**
 * @brief Applies extra resource storage to Pz I harvesters after the tech manager matches tank subtypes.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_HarvesterCapacity : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_HarvesterCapacity();

protected:
	virtual void ApplyOnTank_Internal(ATankMaster* Tank) override;

private:
	void UpgradeHarvesterCapacity(UHarvester& Harvester) const;

	UPROPERTY(EditDefaultsOnly, Category = "Technology|Harvester")
	int32 M_RadixiteExtraCapacity;

	UPROPERTY(EditDefaultsOnly, Category = "Technology|Harvester")
	int32 M_MetalExtraCapacity;
};
