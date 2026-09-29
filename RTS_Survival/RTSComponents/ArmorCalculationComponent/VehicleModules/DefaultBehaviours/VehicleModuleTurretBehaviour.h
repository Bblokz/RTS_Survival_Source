// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBehaviour.h"

#include "VehicleModuleTurretBehaviour.generated.h"

class ATankMaster;
class ACPPTurretsMaster;

/** @brief Assign to Turret yellow/red states to restrict only the turret bound to the module mesh. */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UVehicleModuleTurretBehaviour : public UVehicleModuleBehaviour
{
	GENERATED_BODY()

public:
	UVehicleModuleTurretBehaviour();

protected:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;
	virtual void OnModuleContextUpdated(const FVehicleModuleBehaviourContext& PreviousContext) override;

private:
	void ApplyTraverseRestriction();
	void RemoveTraverseRestriction();
	bool GetIsValidTankMaster() const;
	bool GetIsValidRestrictedTurret() const;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankMaster;

	UPROPERTY()
	TWeakObjectPtr<ACPPTurretsMaster> M_RestrictedTurret;

	bool bM_HasRegisteredTraverseRestriction = false;
};
