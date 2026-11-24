// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BerlinReichstagPatrolRoute.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"

ABerlinReichstagPatrolRoute::ABerlinReichstagPatrolRoute()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	M_EditorSprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("EditorSprite"));
	if (M_EditorSprite)
	{
		M_EditorSprite->SetupAttachment(RootComponent);
		M_EditorSprite->bIsScreenSizeScaled = true;
	}
#endif
}

TArray<FVector> ABerlinReichstagPatrolRoute::GetWorldPatrolPoints() const
{
	TArray<FVector> WorldPoints;
	WorldPoints.Reserve(PatrolPoints.Num());
	const FTransform& ActorTransform = GetActorTransform();
	for (const FVector& LocalPoint : PatrolPoints)
	{
		WorldPoints.Add(ActorTransform.TransformPosition(LocalPoint));
	}
	return WorldPoints;
}
