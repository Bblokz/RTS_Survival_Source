// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "SingleHealBehaviour.generated.h"

/**
 * Adds RepairAmount to the HealthComponent of the owning actor when added only.
 */
UCLASS()
class RTS_SURVIVAL_API USingleHealBehaviour : public UBehaviour
{
	GENERATED_BODY()

public:

	USingleHealBehaviour();
virtual void OnAdded(AActor* BehaviourOwner) override;
	float GetHealing();
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Heal")
	float HealAmount = 10.f;
};
