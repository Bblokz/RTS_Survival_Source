// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/EnergyComponent/EnergyComp.h"

#include "TankEnergyComponent.generated.h"

/**
 * @brief Energy component variant for tank masters that caches tank and turret materials.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTankEnergyComponent : public UEnergyComp
{
	GENERATED_BODY()

protected:
	virtual void CacheOwnerMaterials() override;
};
