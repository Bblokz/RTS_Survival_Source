#include "AnimatedTextPoolActor.h"

AAnimatedTextPoolActor::AAnimatedTextPoolActor()
{
	PrimaryActorTick.bCanEverTick = false;
	AActor::SetActorHiddenInGame(false);
	SetCanBeDamaged(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SetActorEnableCollision(false);
}
