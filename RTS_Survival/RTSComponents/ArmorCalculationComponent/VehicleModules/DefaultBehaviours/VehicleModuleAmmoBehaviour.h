// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"

#include "VehicleModuleAmmoBehaviour.generated.h"

/** @brief Assign to Ammo yellow/red states to slow reloads above 19 mm or prevent new reloads. */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UVehicleModuleAmmoBehaviour : public UBehaviourWeapon
{
	GENERATED_BODY()

public:
	UVehicleModuleAmmoBehaviour();

protected:
	virtual bool CheckRequirement(UWeaponState* WeaponState) const override;
	virtual void ApplyBehaviourToWeapon(UWeaponState* WeaponState) override;
	virtual void RemoveBehaviourFromWeapon(UWeaponState* WeaponState) override;
	virtual void OnModuleContextUpdated(const FVehicleModuleBehaviourContext& PreviousContext) override;
};
