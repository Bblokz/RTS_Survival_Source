// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/RadiusAOEBehaviourComponent/RadiusAOEBehaviourComponent.h"
#include "RadiusRepairAura.generated.h"

/**
 * @brief Radius AOE component that filters targets to those that can be repaired.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API URadiusRepairAura : public URadiusAOEBehaviourComponent
{
	GENERATED_BODY()

protected:
	virtual bool IsValidTarget(AActor* ValidActor) const override;
};
