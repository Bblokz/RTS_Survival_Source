// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** Defines how trigger collision channels respond to player and enemy units. */
UENUM(BlueprintType)
enum class ETriggerOverlapLogic : uint8
{
        OverlapPlayer,
        OverlapEnemy,
        OverlapBoth
};
