// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ESplineDestructionType.generated.h"

/**
 * @brief Selects how a destroyed spline piece collapses.
 * Applies per-system: all pieces on one destructible spline share the same destruction type.
 */
UENUM(BlueprintType)
enum class ESplineDestructionType : uint8
{
	// Sink the piece into the ground over time (FRTS_VerticalCollapse). No geo collection needed.
	VerticalCollapse UMETA(DisplayName = "Vertical Collapse"),
	// Swap the piece for a Chaos Geometry Collection and simulate fracture (FRTS_Collapse::CollapseMesh).
	GeometryCollectionCollapse UMETA(DisplayName = "Geometry Collection Collapse"),
};
