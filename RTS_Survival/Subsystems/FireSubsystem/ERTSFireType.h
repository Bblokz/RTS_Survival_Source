#pragma once

#include "ERTSFireType.generated.h"

/**
 * @brief Fire VFX categories used to drive pooled Niagara component setup.
 * Extend this enum as new fire visuals are added to the game.
 */
UENUM(BlueprintType)
enum class ERTSFireType : uint8
{
	None UMETA(DisplayName="None"),
	Small UMETA(DisplayName="Small"),
	Medium UMETA(DisplayName="Medium"),
	Large UMETA(DisplayName="Large")
};
