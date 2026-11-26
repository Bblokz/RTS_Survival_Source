// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldFowCloud.h"
#include "Components/MeshComponent.h"
#include "EngineUtils.h"

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

float AWorldFowCloud::GetFogTopZ() const
{
	FVector Origin;
	FVector BoxExtent;
	GetActorBounds(false, Origin, BoxExtent);
	return Origin.Z + BoxExtent.Z;
}

bool AWorldFowCloud::TryGetFogTopZ(const UWorld* World, float& OutFogTopZ)
{
	if (not IsValid(World))
	{
		return false;
	}

	for (TActorIterator<AWorldFowCloud> CloudIterator(World); CloudIterator; ++CloudIterator)
	{
		OutFogTopZ = CloudIterator->GetFogTopZ();
		return true;
	}

	return false;
}

void AWorldFowCloud::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	M_CloudMeshComponent = FindComponentByClass<UStaticMeshComponent>();
}
