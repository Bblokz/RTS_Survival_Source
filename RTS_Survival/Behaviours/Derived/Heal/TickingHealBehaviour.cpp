// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TickingHealBehaviour.h"

#include "RTS_Survival/RTSComponents/HealthComponent.h"

UTickingHealBehaviour::UTickingHealBehaviour()
{
	
}

float UTickingHealBehaviour::GetHealingPerSecond() const
{
	return HealAmountPerSecond;
}

void UTickingHealBehaviour::OnAdded(AActor* BehaviourOwner)
{
	Super::OnAdded(BehaviourOwner);
	if(not BehaviourOwner)
	{
	return;		
	}
	M_HealthComponent = BehaviourOwner->FindComponentByClass<UHealthComponent>();
}

void UTickingHealBehaviour::OnTick(const float DeltaTime)
{
	Super::OnTick(DeltaTime);
	if(M_HealthComponent.IsValid())
	{
		M_HealthComponent->Heal(HealAmountPerSecond * DeltaTime);	
	}
}
