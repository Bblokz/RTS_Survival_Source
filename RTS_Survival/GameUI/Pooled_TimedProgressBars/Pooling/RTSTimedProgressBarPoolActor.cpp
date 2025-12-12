
// File: RTSTimedProgressBarPoolActor.cpp
#include "RTSTimedProgressBarPoolActor.h"

ARTSTimedProgressBarPoolActor::ARTSTimedProgressBarPoolActor()
{
	PrimaryActorTick.bCanEverTick = false;
	AActor::SetActorHiddenInGame(false);
	SetCanBeDamaged(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SetActorEnableCollision(false);
}
