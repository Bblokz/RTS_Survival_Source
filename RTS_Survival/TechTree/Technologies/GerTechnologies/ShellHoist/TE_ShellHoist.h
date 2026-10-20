// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TurretWeaponUpgrade/UTE_TurretWeaponUpgradeBase.h"
#include "TE_ShellHoist.generated.h"

/**
 * @brief Selects the heavy assault guns; the behaviour Blueprint shortens the reload of their main gun.
 * @note BehaviourClassToApply: set in the TEBP to the weapon behaviour with the calibre filter and multipliers.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_ShellHoist : public UTE_TurretWeaponUpgradeBase
{
	GENERATED_BODY()

public:
	UTE_ShellHoist();
};
