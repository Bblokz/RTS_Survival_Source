// Copyright (C) Bas Blokzijl - All rights reserved.


#include "InGameUpgradeActor.h"


AInGameUpgradeActor::AInGameUpgradeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled= false;
}

void AInGameUpgradeActor::BeginPlay()
{
	Super::BeginPlay();
	
}


