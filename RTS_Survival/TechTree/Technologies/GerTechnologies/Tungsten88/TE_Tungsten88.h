// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TurretWeaponUpgrade/UTE_TurretWeaponUpgradeBase.h"
#include "TE_Tungsten88.generated.h"

/**
 * @brief Selects the heavy tanks with long 8.8 cm guns; the behaviour Blueprint raises their armour penetration.
 * @note BehaviourClassToApply: set in the TEBP to the weapon behaviour with the calibre filter and multipliers.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_Tungsten88 : public UTE_TurretWeaponUpgradeBase
{
	GENERATED_BODY()

public:
	UTE_Tungsten88();
};
