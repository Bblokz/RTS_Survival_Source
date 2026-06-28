// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldFowCloud.h"
#include "Components/MeshComponent.h"

AWorldFowCloud::AWorldFowCloud()
{
	PrimaryActorTick.bCanEverTick = false;
}

FVector2D AWorldFowCloud::GetMapSizeXY() const
{
	FVector Origin;
	FVector BoxExtent;
	GetActorBounds(false, Origin, BoxExtent);
	return FVector2D(BoxExtent.X * 2.f, BoxExtent.Y * 2.f);
}
