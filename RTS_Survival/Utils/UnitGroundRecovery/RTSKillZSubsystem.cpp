// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSKillZSubsystem.h"

#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSKillZ, Log, All);

namespace RTSKillZStatics
{
	// KillZ this far under the lowest landscape point: deep enough that no unit standing on the map ever reaches it,
	// close enough that a falling unit does within a few seconds.
	constexpr float KillZBelowLandscapeCm = 5000.0f;
	// A KillZ below this was not set by the map (the engine default is -UE_OLD_HALF_WORLD_MAX1).
	constexpr float UnsetKillZThreshold = -0.5f * UE_OLD_HALF_WORLD_MAX1;
}

bool URTSKillZSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void URTSKillZSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	using namespace RTSKillZStatics;
	AWorldSettings* WorldSettings = InWorld.GetWorldSettings();
	if (not IsValid(WorldSettings) || WorldSettings->KillZ > UnsetKillZThreshold)
	{
		// The map chose its own KillZ.
		return;
	}
	FBox LandscapeBounds(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(&InWorld); It; ++It)
	{
		LandscapeBounds += It->GetComponentsBoundingBox(true);
	}
	if (not LandscapeBounds.IsValid)
	{
		// No landscape (menus, mesh-only test maps): keep the engine default.
		return;
	}
	WorldSettings->KillZ = static_cast<float>(LandscapeBounds.Min.Z) - KillZBelowLandscapeCm;
	UE_LOG(LogRTSKillZ, Log, TEXT("KillZ set to %.0f (landscape bottom %.0f) on %s"), WorldSettings->KillZ,
	       LandscapeBounds.Min.Z, *InWorld.GetName());
}
