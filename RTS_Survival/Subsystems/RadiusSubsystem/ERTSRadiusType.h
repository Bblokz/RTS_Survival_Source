#pragma once

#include "ERTSRadiusType.generated.h"

/**
 * @brief Types of RTS helper radii we can render.
 * Keep this lightweight; materials are mapped per type in URadiusPoolSettings.
 */
UENUM(BlueprintType)
enum class ERTSRadiusType : uint8
{
	None                        UMETA(DisplayName="None"),
	Range                       UMETA(DisplayName="Range"),
	PostConstructionBuildingRange UMETA(DisplayName="PostConstructionBuildingRange"),
	Radar                       UMETA(DisplayName="Radar"),
};
