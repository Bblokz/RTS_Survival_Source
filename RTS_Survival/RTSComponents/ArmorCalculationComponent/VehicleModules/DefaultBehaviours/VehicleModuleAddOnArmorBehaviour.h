// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBehaviour.h"

#include "VehicleModuleAddOnArmorBehaviour.generated.h"

/**
 * @brief Default add-on armor effect: scales the add-on contribution of the plates covered by its zone
 * (yellow and red multipliers from VehicleModuleBalance); structural armor is never touched.
 * Assign it per state in the vehicle module asset; hits use pre-impact armor, later hits the new value.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UVehicleModuleAddOnArmorBehaviour : public UVehicleModuleBehaviour
{
	GENERATED_BODY()

public:
	UVehicleModuleAddOnArmorBehaviour();

protected:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;
	virtual void OnModuleContextUpdated(const FVehicleModuleBehaviourContext& PreviousContext) override;

private:
	void ApplyContributionForContext() const;
	float GetContributionMultiplierForState(EVehicleModuleState State) const;
};
