// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_ReconFlight.h"

#include "GlobalAbilityVisionBubble.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"

void UGA_ReconFlight::OnStripFlightProgress(const FStripFlightRun& Run, const float PreviousDistance,
                                            const float NewDistance)
{
	const float Spacing = M_ReconSettings.RevealSpacing;
	const int32 FirstBubble = PreviousDistance < 0.0f ? 0 : FMath::FloorToInt(PreviousDistance / Spacing) + 1;
	const int32 LastBubble = FMath::FloorToInt(NewDistance / Spacing);
	for (int32 BubbleIndex = FirstBubble; BubbleIndex <= LastBubble; ++BubbleIndex)
	{
		SpawnVisionBubble(Run.LineStart + Run.Direction * (BubbleIndex * Spacing));
	}
	const bool bReachedEnd = NewDistance >= Run.StripLength && PreviousDistance < Run.StripLength;
	if (bReachedEnd)
	{
		SpawnVisionBubble(Run.LineStart + Run.Direction * Run.StripLength);
	}
}

void UGA_ReconFlight::SpawnVisionBubble(const FVector& LocationOnStrip) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	FVector GroundLocation;
	GlobalAbilityHelpers::TryFindGroundLocation(this, LocationOnStrip, GroundLocation);
	AGlobalAbilityVisionBubble* Bubble = World->SpawnActor<AGlobalAbilityVisionBubble>(
		AGlobalAbilityVisionBubble::StaticClass(), GroundLocation, FRotator::ZeroRotator);
	if (not IsValid(Bubble))
	{
		return;
	}
	Bubble->StartVision(M_AimSettings.AreaAim.AreaRadius, M_ReconSettings.RevealHoldSeconds,
	                    static_cast<uint8>(GetOwningPlayer()));
}
