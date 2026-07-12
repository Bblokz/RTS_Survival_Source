// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "ReinforcementPathBackupActor.h"

#include "Components/SceneComponent.h"

AReinforcementPathBackupActor::AReinforcementPathBackupActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	M_RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(M_RootComponent);
}
