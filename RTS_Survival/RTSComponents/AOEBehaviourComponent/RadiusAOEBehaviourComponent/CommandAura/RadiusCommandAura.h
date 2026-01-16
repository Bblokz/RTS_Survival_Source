// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/RadiusAOEBehaviourComponent/RadiusAOEBehaviourComponent.h"
#include "RadiusCommandAura.generated.h"

/**
 * @brief Adds a permanent command aura that improves allied weapon performance in range.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API URadiusCommandAura : public URadiusAOEBehaviourComponent
{
	GENERATED_BODY()

public:
	URadiusCommandAura();

protected:
	virtual void SetHostBehaviourUIData(UBehaviour& Behaviour) const override;

private:
	float GetTotalAccuracyPercentageGain() const;
	float GetTotalReloadTimePercentageReduction() const;
};
