// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"

#include "BehaviourTurretRotation.generated.h"

class ATankMaster;
class ACPPTurretsMaster;

/**
 * @brief Behaviour that modifies turret rotation speeds on owning tanks using additive and multiplicative tuning.
 */
UCLASS()
class RTS_SURVIVAL_API UTurretRotationBehaviour : public UBehaviour
{
	GENERATED_BODY()

public:
	UTurretRotationBehaviour();

protected:
	virtual void OnAdded() override;
	virtual void OnRemoved() override;
	virtual void OnStack(UBehaviour* StackedBehaviour) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behaviour|TurretRotation", meta = (AllowPrivateAccess = "true"))
	float RotationSpeedMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behaviour|TurretRotation", meta = (AllowPrivateAccess = "true"))
	float RotationSpeedAdd = 0.0f;

private:
	void ApplyBehaviourToTurrets();
	void ApplyBehaviourToTurret(ACPPTurretsMaster* Turret);
	void RemoveBehaviourFromTurrets();
	void ClearTrackedTurrets();
	float CalculateRotationSpeedChange(const float BaseRotationSpeed) const;
	bool TryGetTankMaster(ATankMaster*& OutTankMaster);
	bool GetIsValidTankMaster() const;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankMaster;

	UPROPERTY()
	TArray<TWeakObjectPtr<ACPPTurretsMaster>> M_AffectedTurrets;

	UPROPERTY()
	TMap<TWeakObjectPtr<ACPPTurretsMaster>, float> M_AppliedRotationChanges;

	bool bM_HasAppliedRotationAtLeastOnce = false;
	bool bM_IsBehaviourActive = false;
};
