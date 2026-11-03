// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalMapSite.h"

#include "Components/SceneComponent.h"

ASurvivalMapSite::ASurvivalMapSite()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorHiddenInGame(true);
}
