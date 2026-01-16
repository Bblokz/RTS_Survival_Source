// Copyright (C) Bas Blokzijl - All rights reserved.


#include "SingleHealBehaviour.h"

#include "RTS_Survival/RTSComponents/HealthComponent.h"

USingleHealBehaviour::USingleHealBehaviour()
{
        BehaviourLifeTime = EBehaviourLifeTime::Timed;
	M_LifeTimeDuration = 1.f;
}

void USingleHealBehaviour::OnAdded(AActor* BehaviourOwner)
{
	Super::OnAdded(BehaviourOwner);
	if(!IsValid(BehaviourOwner))
	{
		return;
	}
	UHealthComponent* HealthComp = BehaviourOwner->FindComponentByClass<UHealthComponent>();
	if(not HealthComp)
	{
		return;
	}
	HealthComp->Heal(10.f);
}

float USingleHealBehaviour::GetHealing()
{
	return HealAmount;
}
