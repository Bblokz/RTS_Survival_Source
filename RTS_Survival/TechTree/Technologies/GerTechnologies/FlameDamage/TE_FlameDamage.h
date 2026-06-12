// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_FlameDamage.generated.h"

class AInfantryWeaponMaster;
class ASquadController;
class ASquadUnit;
class ATankMaster;
class ACPPTurretsMaster;
class UHullWeaponComponent;
class UWeaponState;

/**
 * @brief Recolours flame weapon VFX on eligible researched units while preserving weapon behaviour.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_FlameDamage : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_FlameDamage();

protected:
	virtual void ApplyOnTank_Internal(ATankMaster* Tank) override;
	virtual void ApplyOnSquad_Internal(ASquadController* Squad) override;

private:
	void ApplyFlameColorToTankTurrets(ATankMaster& Tank) const;
	void ApplyFlameColorToTankHullWeapons(ATankMaster& Tank) const;
	void ApplyFlameColorToSquadUnit(ASquadUnit& SquadUnit) const;
	void ApplyFlameColorToInfantryWeapon(AInfantryWeaponMaster& InfantryWeapon) const;
	void ApplyFlameColorToTurret(ACPPTurretsMaster& Turret) const;
	void ApplyFlameColorToHullWeapon(UHullWeaponComponent& HullWeapon) const;
	void ApplyFlameColorToWeapons(const TArray<UWeaponState*>& Weapons) const;

	UPROPERTY(EditDefaultsOnly, Category = "Technology|Flame Damage")
	FLinearColor M_FlameColor;
};
