// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"

#include "VehicleModuleWeaponBehaviour.generated.h"

/** @brief Applies yellow dispersion/cycle penalties or a red firing lock to the bound gun only. */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UVehicleModuleWeaponBehaviour : public UBehaviourWeapon
{
	GENERATED_BODY()

public:
	UVehicleModuleWeaponBehaviour();

protected:
	virtual bool CheckRequirement(UWeaponState* WeaponState) const override;
	virtual void ApplyBehaviourToWeapon(UWeaponState* WeaponState) override;
	virtual void RemoveBehaviourFromWeapon(UWeaponState* WeaponState) override;
	virtual void OnModuleContextUpdated(const FVehicleModuleBehaviourContext& PreviousContext) override;

private:
	UPROPERTY()
	TMap<TWeakObjectPtr<UWeaponState>, int32> M_AccuracyDeltaByWeapon;
};
