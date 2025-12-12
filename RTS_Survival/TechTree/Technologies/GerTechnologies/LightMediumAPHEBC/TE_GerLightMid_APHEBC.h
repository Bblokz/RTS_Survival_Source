// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TurretWeaponUpgrade/UTE_TurretWeaponUpgradeBase.h"
#include "TE_GerLightMid_APHEBC.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTE_GerLightMid_APHEBC : public UTE_TurretWeaponUpgradeBase
{
	GENERATED_BODY()

protected:
	virtual void ApplyEffectToWeapon(FWeaponData* ValidWeaponData) override;
};
