// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/TacticalAIComponents/TacticalAIComp.h"
#include "EnemyTankTacticalAI.generated.h"


class ACPPTurretsMaster;
class ATankMaster;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnemyTankTacticalAI : public UTacticalAIComp
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEnemyTankTacticalAI();

	virtual void OnUnitInCombat() override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankOwner  =nullptr;

	UPROPERTY()
	TWeakObjectPtr<ACPPTurretsMaster> M_TankTurret = nullptr;

	ACPPTurretsMaster* SetOrGetTurret();
	bool GetIsValidTankOwner() const
	{
		return M_TankOwner.IsValid();
	}

	bool IsOwnerIdle() const;
	bool IsOwnerAttackingAndInRangeOfTarget();
	bool GetIsTurretInRangeOfTarget();

	void OnTankIdleInCombat();
	bool RotateTankToTurretTargetIfValid();
	

	const float M_TacticalCombatInterval = 2.0f;
	float M_LastTacticalCombatActionTime = 0.0f;
	

};
