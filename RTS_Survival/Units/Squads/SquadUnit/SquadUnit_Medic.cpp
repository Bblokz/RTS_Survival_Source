// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SquadUnit_Medic.h"

#include "RTS_Survival/RTSComponents/MedicComponent/MedicComponent.h"

ASquadUnit_Medic::ASquadUnit_Medic(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The base unit finds this component in PostInitializeComponents and hands itself to it.
	MedicComponent = CreateDefaultSubobject<UMedicComponent>(TEXT("MedicComponent"));
}
