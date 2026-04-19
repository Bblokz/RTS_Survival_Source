// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExperienceLevelBehaviour.h"

#include "AircraftExperienceBehaviour.generated.h"

/**
 * @brief Experience behaviour for aircraft units.
 * Uses weapon/health/vision multipliers from UExperienceLevelBehaviour.
 */
UCLASS()
class RTS_SURVIVAL_API UAircraftExperienceBehaviour : public UExperienceLevelBehaviour
{
	GENERATED_BODY()
};
