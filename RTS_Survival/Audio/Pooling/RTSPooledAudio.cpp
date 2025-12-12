// RTSPooledAudio.cpp
#include "RTSPooledAudio.h"

#include "Components/SceneComponent.h"

ARTSPooledAudio::ARTSPooledAudio()
{
	PrimaryActorTick.bCanEverTick = false;
	AActor::SetActorHiddenInGame(true);
	SetCanBeDamaged(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SetActorEnableCollision(false);
}
