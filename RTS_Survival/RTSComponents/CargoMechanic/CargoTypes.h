// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CargoTypes.generated.h"

/**
 * @brief High-level cargo state for a squad.
 * None/Free are both "not inside"; Inside means the squad is currently docked in cargo.
 */
UENUM(BlueprintType)
enum class ESquadCargoState : uint8
{
	None   UMETA(DisplayName="None"),
	Free   UMETA(DisplayName="Free"),
	Inside UMETA(DisplayName="Inside")
};
