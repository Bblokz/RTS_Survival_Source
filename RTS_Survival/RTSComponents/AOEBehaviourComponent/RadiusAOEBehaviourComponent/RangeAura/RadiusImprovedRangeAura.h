// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/RadiusAOEBehaviourComponent/RadiusAOEBehaviourComponent.h"
#include "RadiusImprovedRangeAura.generated.h"

/**
 * @brief Adds a permanent weapon range buff aura with radius-based behaviour display.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API URadiusImprovedRangeAura : public URadiusAOEBehaviourComponent
{
	GENERATED_BODY()

public:
	URadiusImprovedRangeAura();

protected:
	virtual void SetHostBehaviourUIData(UBehaviour& Behaviour) const override;

private:
	float GetTotalRangePercentageGain() const;
};
