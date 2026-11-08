// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSRenderPerf.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

void RTSRenderPerf::ExcludeSmallAttachmentsFromDistanceFields(const AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(Actor);
	for (UStaticMeshComponent* const MeshComponent : MeshComponents)
	{
		if (not IsValid(MeshComponent))
		{
			continue;
		}
		if (MeshComponent->Mobility != EComponentMobility::Movable)
		{
			MeshComponent->SetMobility(EComponentMobility::Movable);
		}
		const bool bNeedsChange = MeshComponent->bAffectDistanceFieldLighting ||
			MeshComponent->bAffectDynamicIndirectLighting;
		if (not bNeedsChange)
		{
			continue;
		}
		MeshComponent->bAffectDistanceFieldLighting = false;
		MeshComponent->bAffectDynamicIndirectLighting = false;
		MeshComponent->MarkRenderStateDirty();
	}
}
