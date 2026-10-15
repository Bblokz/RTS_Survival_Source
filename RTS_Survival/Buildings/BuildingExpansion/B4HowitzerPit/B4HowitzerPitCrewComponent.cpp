// Copyright (C) Bas Blokzijl - All rights reserved.

#include "B4HowitzerPitCrewComponent.h"

UB4HowitzerPitCrewComponent::UB4HowitzerPitCrewComponent()
{
	CrewTag = TEXT("B4PitCrew");
	PropSocketTagPrefix = TEXT("B4PitSocket:");
	CrewAttachSocket = TEXT("PitOrigin");
	bKeepCrewBlueprintOffset = false;
	// The B-4 shell rides on the gun's crane through the gun reload montage; no rounds change hands.
	ShellCarry.RoundTag = NAME_None;
}
