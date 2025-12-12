// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GroundEnvActor.h"

#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"


// Sets default values
AGroundEnvActor::AGroundEnvActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// Called when the game starts or when spawned
void AGroundEnvActor::BeginPlay()
{
	Super::BeginPlay();
	
}

void AGroundEnvActor::SetupCollision(TArray<UMeshComponent*> Components, const bool bAllowBuilding, const bool bAffectNavMesh)
{
	for(auto EachMesh: Components)
	{
		FRTS_CollisionSetup::SetupGroundEnvActorCollision(EachMesh, bAllowBuilding, bAffectNavMesh);
	}
}


