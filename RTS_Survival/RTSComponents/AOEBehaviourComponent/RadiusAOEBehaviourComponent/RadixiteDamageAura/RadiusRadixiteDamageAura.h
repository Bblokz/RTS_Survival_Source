// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/RadiusAOEBehaviourComponent/RadiusAOEBehaviourComponent.h"
#include "RadiusRadixiteDamageAura.generated.h"

class URadixiteDamageBehaviour;
struct FBehaviourUIData;

/**
 * @brief Radius AOE component that applies Radixite damage-over-time behaviours to enemy units in range.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API URadiusRadixiteDamageAura : public URadiusAOEBehaviourComponent
{
	GENERATED_BODY()

public:
	URadiusRadixiteDamageAura();

protected:
	virtual void SetHostBehaviourUIData(UBehaviour& Behaviour) const override;

private:
	float GetDamagePerSecondFromBehaviours() const;
	void SetDescriptionWithDamageAmount(FBehaviourUIData& OutUIData, float DamagePerSecond) const;
};
