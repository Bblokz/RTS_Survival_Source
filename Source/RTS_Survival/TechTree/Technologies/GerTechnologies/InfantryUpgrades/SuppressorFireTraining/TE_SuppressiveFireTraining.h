// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/InfantryWeaponUpgrade/TE_InfantryWeaponUpgradeBase.h"
#include "TE_SuppressiveFireTraining.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTE_SuppressiveFireTraining : public UTE_InfantryWeaponUpgradeBase
{
	GENERATED_BODY()

protected:
	virtual void ApplyEffectToWeapon(FWeaponData* ValidWeaponData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float M_RateOfFireMlt = 0.75;
};
