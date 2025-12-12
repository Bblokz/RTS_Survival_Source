// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WallActor.h"

#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"


// Sets default values
AWallActor::AWallActor(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWallActor::BeginPlay()
{
	Super::BeginPlay();
	
}

void AWallActor::SetupWallCollision(UStaticMeshComponent* WallMeshComponent,
	const bool bAffectNavMesh)
{
	if(not GetIsValidRTSComponent())
	{
		return;
	}
	FRTS_CollisionSetup::SetupWallActorMeshCollision(WallMeshComponent, bAffectNavMesh, RTSComponent->GetOwningPlayer());
}

void AWallActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	RTSComponent = FindComponentByClass<URTSComponent>();
	(void)GetIsValidRTSComponent();
	
}

bool AWallActor::GetIsValidRTSComponent() const
{
	if(not IsValid(RTSComponent))
	{
		RTSFunctionLibrary::PrintString("Invalid rts component in WallActor");
		return false;
	}
	return true;
}


