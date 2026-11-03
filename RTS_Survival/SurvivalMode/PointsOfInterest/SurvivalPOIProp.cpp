// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalPOIProp.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "NavAreas/NavArea_Null.h"
#include "NavModifierComponent.h"

ASurvivalPOIProp::ASurvivalPOIProp()
{
	PrimaryActorTick.bCanEverTick = false;
	M_Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	M_Mesh->SetMobility(EComponentMobility::Movable);
	M_Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	RootComponent = M_Mesh;
	M_NavModifier = CreateDefaultSubobject<UNavModifierComponent>(TEXT("NavModifier"));
	M_NavModifier->SetAreaClass(UNavArea_Null::StaticClass());
}

void ASurvivalPOIProp::InitProp(UStaticMesh* Mesh, const bool bBlocksNavigation)
{
	M_Mesh->SetStaticMesh(Mesh);
	if (bBlocksNavigation)
	{
		return;
	}
	M_Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_Mesh->SetCanEverAffectNavigation(false);
	M_NavModifier->SetCanEverAffectNavigation(false);
	M_NavModifier->SetAreaClass(nullptr);
}
