// Copyright (C) Bas Blokzijl - All rights reserved.


#include "CPPMonstersMaster.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"

ACPPMonstersMaster::ACPPMonstersMaster(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

void ACPPMonstersMaster::UnitDies(const ERTSDeathType DeathType)
{
	UnitDiesBP();
	HealthComponent->MakeHealthBarInvisible();
}
