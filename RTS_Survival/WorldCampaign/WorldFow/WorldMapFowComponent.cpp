// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldMapFowComponent.h"

UWorldMapFowComponent::UWorldMapFowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	M_ExplorableSettings.bM_WritesExplorableMask = true;
	M_VisibleSettings.bM_WritesVisibleMask = true;
	M_POIVisibleSettings.bM_WritesPOIMask = true;
	M_POIVisibleSettings.M_RevealRadius = M_POIRevealRadius;
	M_POIVisibleSettings.M_RevealFalloff = M_POIRevealFalloff;
}

void UWorldMapFowComponent::SetCurrentFowState(const EWorldMapFowState NewState)
{
	M_CurrentFowState = NewState;
}

bool UWorldMapFowComponent::GetCanPrimaryClickInteractForCurrentState() const
{
	return M_CurrentFowState == EWorldMapFowState::Explorable || M_CurrentFowState == EWorldMapFowState::Visible;
}

bool UWorldMapFowComponent::GetCanBeOutlinedForCurrentState() const
{
	return M_CurrentFowState == EWorldMapFowState::Explorable || M_CurrentFowState == EWorldMapFowState::Visible || M_CurrentFowState == EWorldMapFowState::POIVisible;
}

bool UWorldMapFowComponent::GetWritesVisibleMaskForCurrentState() const { return GetSettingsForState(M_CurrentFowState).bM_WritesVisibleMask; }
bool UWorldMapFowComponent::GetWritesExplorableMaskForCurrentState() const { return GetSettingsForState(M_CurrentFowState).bM_WritesExplorableMask; }
bool UWorldMapFowComponent::GetWritesPOIMaskForCurrentState() const { return GetSettingsForState(M_CurrentFowState).bM_WritesPOIMask; }
float UWorldMapFowComponent::GetRevealRadiusForCurrentState() const { return GetSettingsForState(M_CurrentFowState).M_RevealRadius; }
float UWorldMapFowComponent::GetRevealFalloffForCurrentState() const { return GetSettingsForState(M_CurrentFowState).M_RevealFalloff; }
float UWorldMapFowComponent::GetConnectionCorridorWidthForCurrentState() const { return GetSettingsForState(M_CurrentFowState).M_ConnectionCorridorWidth; }

FVector UWorldMapFowComponent::GetPOIRevealOrigin() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) ? Owner->GetActorLocation() + M_POIRevealOriginOffset : M_POIRevealOriginOffset;
}

const FWorldMapFowStateSettings& UWorldMapFowComponent::GetSettingsForState(const EWorldMapFowState State) const
{
	switch (State)
	{
	case EWorldMapFowState::Explorable: return M_ExplorableSettings;
	case EWorldMapFowState::Visible: return M_VisibleSettings;
	case EWorldMapFowState::POIVisible: return M_POIVisibleSettings;
	case EWorldMapFowState::NotVisible:
	default: return M_NotVisibleSettings;
	}
}
