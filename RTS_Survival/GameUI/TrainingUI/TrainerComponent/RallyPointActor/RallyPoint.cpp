// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RallyPoint.h"


ARTSRallyPointActor::ARTSRallyPointActor()
{
	PrimaryActorTick.bCanEverTick = false;

	M_SimpleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(M_SimpleRoot);

	// This actor is only a marker; keep collisions off by default.
	SetActorEnableCollision(false);
}

void ARTSRallyPointActor::BeginPlay()
{
	Super::BeginPlay();
	// Nothing special; visibility controlled by owning TrainerComponent.
}
