// Copyright (C) Bas Blokzijl - All rights reserved.


#include "FieldConstruction.h"

#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"


// Sets default values
AFieldConstruction::AFieldConstruction(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AFieldConstruction::BeginPlay()
{
	Super::BeginPlay();
}

void AFieldConstruction::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	FieldConstructionMesh = FindComponentByClass<UStaticMeshComponent>();
}

bool AFieldConstruction::GetIsValidFieldConstructionMesh()
{
	if (not IsValid(FieldConstructionMesh))
	{
		return false;
	}
	return true;
}

void AFieldConstruction::SetupCollision(const int32 OwningPlayer, const bool bBlockPlayerProjectiles)
{
	TArray<UMeshComponent*> MeshComponents;
	GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* MeshComp : MeshComponents)
	{
		if (not IsValid(MeshComp))
		{
			continue;
		}
		FRTS_CollisionSetup::SetupFieldConstructionMeshCollision(MeshComp, OwningPlayer, bBlockPlayerProjectiles);
	}
}
