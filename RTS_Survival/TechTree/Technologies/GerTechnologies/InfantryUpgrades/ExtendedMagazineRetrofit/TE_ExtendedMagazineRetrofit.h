// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/InfantryWeaponUpgrade/TE_InfantryWeaponUpgradeBase.h"
#include "TE_ExtendedMagazineRetrofit.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTE_ExtendedMagazineRetrofit : public UTE_InfantryWeaponUpgradeBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TMap<EWeaponName, int> WeaponMagazineSizeIncrease;

	virtual void ApplyEffectToWeapon(FWeaponData* ValidWeaponData) override;
};
