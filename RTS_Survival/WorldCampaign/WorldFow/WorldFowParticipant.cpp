// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldFowParticipant.h"
#include "WorldMapFowComponent.h"

bool IWorldFowParticipant::GetCanBeOutlined() const
{
	const UWorldMapFowComponent* FowComponent = GetFowComponent();
	return IsValid(FowComponent) && FowComponent->GetCanBeOutlinedForCurrentState();
}

bool IWorldFowParticipant::GetCanPrimaryClickInteract() const
{
	const UWorldMapFowComponent* FowComponent = GetFowComponent();
	return IsValid(FowComponent) && FowComponent->GetCanPrimaryClickInteractForCurrentState();
}
