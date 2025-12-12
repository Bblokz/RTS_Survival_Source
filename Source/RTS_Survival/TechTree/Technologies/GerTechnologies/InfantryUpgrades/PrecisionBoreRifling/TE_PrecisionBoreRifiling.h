// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/InfantryWeaponUpgrade/TE_InfantryWeaponUpgradeBase.h"
#include "TE_PrecisionBoreRifiling.generated.h"

/**
 * Improves the accuracy of some infantry weapons. 
 */
UCLASS()
class RTS_SURVIVAL_API UTE_PrecisionBoreRifiling : public UTE_InfantryWeaponUpgradeBase
{
	GENERATED_BODY()

protected:
	virtual void ApplyEffectToWeapon(FWeaponData* ValidWeaponData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float M_AccuracyMlt = 1.15f;
};
