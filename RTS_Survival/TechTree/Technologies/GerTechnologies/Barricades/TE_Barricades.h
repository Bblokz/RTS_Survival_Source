// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_Barricades.generated.h"

class ANomadicVehicle;

/**
 * @brief Applies the researched barricades building-health bonus to eligible German nomadic buildings.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_Barricades : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_Barricades();

protected:
	virtual void ApplyOnNomadic_Internal(ANomadicVehicle* Nomadic) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Technology|Barricades")
	float M_HealthMultiplier;
};
