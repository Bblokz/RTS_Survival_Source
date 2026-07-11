// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "ReinforcementPathActor.h"

#include "Components/SceneComponent.h"

AReinforcementPathActor::AReinforcementPathActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	M_RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(M_RootComponent);
}
