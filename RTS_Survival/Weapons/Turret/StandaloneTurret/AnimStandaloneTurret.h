// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/Turret/TurretAnimInstance/TurretAnimInstance.h"

#include "AnimStandaloneTurret.generated.h"

/**
 * @brief Animation-instance base used by standalone turret blueprints.
 * The owning turret writes local yaw and pitch while the animation graph applies them to the weapon bones.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UAnimStandaloneTurret : public UTurretAnimInstance
{
	GENERATED_BODY()
};
