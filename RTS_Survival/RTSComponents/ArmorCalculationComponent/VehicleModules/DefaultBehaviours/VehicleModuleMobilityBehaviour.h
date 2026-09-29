// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBehaviour.h"

#include "VehicleModuleMobilityBehaviour.generated.h"

class ATankMaster;

/**
 * @brief Default running gear/engine effect: while its module is yellow or red it registers a travel and turn
 * limit on the owning tank (values from VehicleModuleBalance). Assign it per state in the vehicle module asset.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UVehicleModuleMobilityBehaviour : public UVehicleModuleBehaviour
{
	GENERATED_BODY()

public:
	UVehicleModuleMobilityBehaviour();

protected:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;
	virtual void OnModuleContextUpdated(const FVehicleModuleBehaviourContext& PreviousContext) override;

private:
	void ApplyMobilityRestriction();
	/**
	 * @brief Keeps engine and running-gear limits in one source-owned mobility update.
	 * @param OutTravelSpeedMultiplier Travel limit selected for the current state.
	 * @param OutTurnRateMultiplier Turning limit selected for the current state.
	 * @param OutAccelerationMultiplier Acceleration limit selected for the current state.
	 */
	void GetMobilityMultipliersForContext(float& OutTravelSpeedMultiplier, float& OutTurnRateMultiplier,
	                                     float& OutAccelerationMultiplier) const;
	bool GetIsValidTankMaster() const;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankMaster;
};
