// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_LightTanksTracksImprovement.generated.h"

class ATankMaster;

/**
 * @brief Applies mobility changes directly to eligible loaded tanks after the tech manager matches subtypes.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_LightTanksTracksImprovement : public UTechnologyEffect
{
	GENERATED_BODY()

protected:
	virtual void ApplyOnTank_Internal(ATankMaster* Tank) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	float MaxSpeedMlt = 1.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	float TurnRateMlt = 1.25f;
};
