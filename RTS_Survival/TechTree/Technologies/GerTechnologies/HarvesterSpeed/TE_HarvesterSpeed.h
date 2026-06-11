// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_HarvesterSpeed.generated.h"

class UHarvester;
class ATankMaster;

/**
 * @brief Applies faster harvesting animation playback to Pz I harvesters after subtype matching.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_HarvesterSpeed : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_HarvesterSpeed();

protected:
	virtual void ApplyOnTank_Internal(ATankMaster* Tank) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Technology|Harvester")
	float M_HarvesterAnimationSpeedPlayRate;
};
