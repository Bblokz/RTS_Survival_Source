// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_Spalliners.generated.h"

class ATankMaster;

/**
 * @brief Applies a survivability boost to researched medium German armour variants when the tech manager resolves them.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_Spalliners : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_Spalliners();

protected:
	virtual void ApplyOnTank_Internal(ATankMaster* Tank) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Technology|Spalliners")
	float M_HealthMultiplier;
};
