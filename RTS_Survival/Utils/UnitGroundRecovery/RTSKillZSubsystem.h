// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSKillZSubsystem.generated.h"

/**
 * @brief Sets each game map's KillZ just under its landscape when the map left it at the engine default (about
 * -10 km), so a unit that falls through the world reaches it within seconds instead of minutes. The engine already
 * compares every moving unit against KillZ (character movement, physics sync) and calls FellOutOfWorld; the unit
 * classes override that to put the unit back on the ground (FRTSUnitGroundRecovery). One landscape bounds pass when
 * the map starts; nothing per frame.
 */
UCLASS()
class RTS_SURVIVAL_API URTSKillZSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
