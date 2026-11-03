// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBaseLantern.h"

#include "Components/StaticMeshComponent.h"

ASurvivalSovietBaseLantern::ASurvivalSovietBaseLantern()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	M_Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(M_Mesh);
	M_Mesh->SetMobility(EComponentMobility::Movable);
	M_Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_Mesh->SetCanEverAffectNavigation(false);
}

void ASurvivalSovietBaseLantern::InitLantern(UStaticMesh* Mesh)
{
	M_Mesh->SetStaticMesh(Mesh);
}
