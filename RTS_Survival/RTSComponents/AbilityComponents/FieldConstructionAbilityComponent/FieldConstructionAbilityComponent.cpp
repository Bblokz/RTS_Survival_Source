// Copyright (C) Bas Blokzijl - All rights reserved.


#include "FieldConstructionAbilityComponent.h"


UFieldConstructionAbilityComponent::UFieldConstructionAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


void UFieldConstructionAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
}



