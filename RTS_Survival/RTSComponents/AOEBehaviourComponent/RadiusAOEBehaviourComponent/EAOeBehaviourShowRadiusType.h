// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "EAOeBehaviourShowRadiusType.generated.h"

/**
 * @brief Controls when a radius AOE component should display its radius.
 */
UENUM(BlueprintType)
enum class EAOeBehaviourShowRadiusType : uint8
{
	None UMETA(DisplayName="None"),
	OnHover UMETA(DisplayName="OnHover"),
	OnSelectionOnly UMETA(DisplayName="OnSelectionOnly"),
	OnHoverAndOnSelection UMETA(DisplayName="OnHoverAndOnSelection")
};
