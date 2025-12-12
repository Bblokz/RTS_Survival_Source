// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "RTS_Survival/TechTree/Technologies/TurretWeaponUpgrade/UTE_TurretWeaponUpgradeBase.h"
#include "TE_AutoCannonAPCR.generated.h"

enum class EWeaponName : uint8;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTE_AutoCannonAPCR : public UTE_TurretWeaponUpgradeBase
{
	GENERATED_BODY()


protected:
	virtual void ApplyEffectToWeapon(FWeaponData* ValidWeaponData) override;

};
