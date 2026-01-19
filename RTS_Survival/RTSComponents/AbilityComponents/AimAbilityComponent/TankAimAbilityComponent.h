// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityComponent.h"
#include "TankAimAbilityComponent.generated.h"

class ATankMaster;
class ACPPTurretsMaster;

/**
 * @brief Aim ability component used by tank masters to apply effects to a cached turret.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTankAimAbilityComponent : public UAimAbilityComponent
{
	GENERATED_BODY()

public:
	UTankAimAbilityComponent();

protected:
	virtual void BeginPlay() override;
	virtual void PostInitProperties() override;

	virtual bool CollectWeaponStates(TArray<UWeaponState*>& OutWeaponStates, float& OutMaxRange) const override;
	virtual void SetWeaponsDisabled() override;
	virtual void SetWeaponsAutoEngage(const bool bUseLastTarget) override;
	virtual void FireWeaponsAtLocation(const FVector& TargetLocation) override;
	virtual void RequestMoveToLocation(const FVector& MoveToLocation) override;
	virtual void StopMovementForAbility() override;
	virtual void BeginAbilityDurationTimer() override;
	virtual void ClearDerivedTimers() override;

private:
	void BeginPlay_CacheTurretNextTick();
	void CacheTurretWithMostRange();
	float GetTurretMaxRange(const ACPPTurretsMaster* Turret, TArray<UWeaponState*>& OutWeaponStates) const;
	void CheckTurretRotationAndStartBehaviour();
	void ClearTurretRotationTimer();

	bool GetIsValidTankMaster() const;
	bool GetIsValidTurretWithMostRange() const;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankMaster;

	UPROPERTY()
	TWeakObjectPtr<ACPPTurretsMaster> M_TurretWithMostRange;

	FTimerHandle M_TurretRotationCheckTimerHandle;
};
