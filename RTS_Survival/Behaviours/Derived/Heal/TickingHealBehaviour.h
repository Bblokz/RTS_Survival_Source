// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "UObject/Object.h"
#include "TickingHealBehaviour.generated.h"

class UHealthComponent;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTickingHealBehaviour : public UBehaviour
{
	GENERATED_BODY()
public:

	UTickingHealBehaviour();
	float GetHealingPerSecond()const;

	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnTick(const float DeltaTime)override;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Heal")
	float HealAmountPerSecond = 10.f;

private:
	UPROPERTY()
	TWeakObjectPtr<UHealthComponent> M_HealthComponent;
};
