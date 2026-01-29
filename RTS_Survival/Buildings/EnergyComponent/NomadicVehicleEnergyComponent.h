// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/EnergyComponent/EnergyComp.h"

#include "NomadicVehicleEnergyComponent.generated.h"

/**
 * @brief Energy component for nomadic vehicles that only applies low energy behaviours.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UNomadicVehicleEnergyComponent : public UEnergyComp
{
	GENERATED_BODY()

protected:
	virtual void CacheOwnerMaterials() override;
	virtual bool GetShouldApplyMaterialChanges() const override;
};
