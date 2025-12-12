// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "TE_LongRangeCalibration.generated.h"

class UWeaponState;
enum class EWeaponName : uint8;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTE_LongRangeCalibration : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	virtual void ApplyTechnologyEffect(const UObject* WorldContextObject) override;

protected:
	virtual void OnApplyEffectToActor(AActor* ValidActor) override;

	virtual TSet<TSubclassOf<AActor>> GetTargetActorClasses() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSet<TSubclassOf<AActor>> M_AffectedTurrets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSet<EWeaponName> M_AffectedWeapons;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float M_RangeMlt = 1.2f;

private:
	void ApplyeffectToWeapon(UWeaponState* ValidWeaponState, ACPPTurretsMaster* ValidTurret);
	void UpgradeGameStateForAffectedWeapons(ACPPTurretsMaster* ValidTurret, const int32 OwningPlayer);

	bool bHasUpgradedGameState = false;
};
