// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"

#include "RTSStandaloneTurretOptimizer.generated.h"

class AStandaloneTurret;

/**
 * @brief Added to standalone turrets so their combat simulation remains active while presentation work scales down.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSStandaloneTurretOptimizer : public URTSOptimizer
{
	GENERATED_BODY()

public:
	URTSStandaloneTurretOptimizer();

protected:
	virtual void DetermineOwnerOptimization(AActor* ValidOwner) override;
	virtual void InFOVUpdateComponents() override;
	virtual void OutFovCloseUpdateComponents() override;
	virtual void OutFovFarUpdateComponents() override;

private:
	UPROPERTY()
	TWeakObjectPtr<AStandaloneTurret> M_StandaloneTurret;

	float M_BaseTurretTickInterval = 0.0f;

	/**
	 * @brief Keeps the actor's logical aim alive while telling it whether presentation may run.
	 * @param TickInterval Actor tick interval for the requested optimization state.
	 * @param OptimizationDistance State whose presentation policy the turret should apply.
	 */
	void ApplyTurretOptimization(
		float TickInterval,
		ERTSOptimizationDistance OptimizationDistance) const;

	bool GetIsValidStandaloneTurret() const;
};
