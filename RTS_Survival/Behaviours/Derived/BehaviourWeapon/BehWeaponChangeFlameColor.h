// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"

#include "BehWeaponChangeFlameColor.generated.h"

class UWeaponState;
class UWeaponStateFlameThrower;

/**
 * @brief Weapon behaviour used to override flamethrower flame colors on eligible weapons.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UBehWeaponChangeFlameColor : public UBehaviourWeapon
{
	GENERATED_BODY()

public:
	UBehWeaponChangeFlameColor();

protected:
	virtual bool CheckRequirement(UWeaponState* WeaponState) const override;
	virtual void ApplyBehaviourToWeapon(UWeaponState* WeaponState) override;
	virtual void RemoveBehaviourFromWeapon(UWeaponState* WeaponState) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behaviour|Flame")
	FLinearColor M_FlameColorOverride = FLinearColor::White;

private:
	void ApplyColorOverride(UWeaponStateFlameThrower* FlameThrowerWeapon);
	void RestoreColorOverride(UWeaponStateFlameThrower* FlameThrowerWeapon);
	bool TryGetCachedColor(const TWeakObjectPtr<UWeaponStateFlameThrower>& WeaponPtr,
		FLinearColor& OutColor) const;

	UPROPERTY()
	TMap<TWeakObjectPtr<UWeaponStateFlameThrower>, FLinearColor> M_PreviousFlameColors;
};
