// Copyright (C) Bas Blokzijl - All rights reserved.


#include "DestructibleBridge.h"

#include "RTS_Survival/RTSCollisionTraceChannels.h"


ADestructibleBridge::ADestructibleBridge(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer)
{
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bCanEverTick = false;
}

void ADestructibleBridge::BeginPlay()
{
	Super::BeginPlay();
}

void ADestructibleBridge::KillActorsOnBridge(UMeshComponent* BridgeMesh)
{
	if (not IsValid(BridgeMesh))
	{
		return;
	}
	TArray<ECollisionChannel> CollisionChannelsToCheck =
	{
		ECC_Pawn,
		ECC_PhysicsBody,
		ECC_Destructible,
		COLLISION_OBJ_PLAYER,
		COLLISION_OBJ_ENEMY
	};
}
