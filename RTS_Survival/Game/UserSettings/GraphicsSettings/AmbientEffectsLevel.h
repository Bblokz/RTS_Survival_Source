#pragma once

#include "CoreMinimal.h"

#include "AmbientEffectsLevel.generated.h"

/**
 * @brief How many ambient landscape effects (dust devils on the desert maps) may be alive around the camera.
 * The value is handed to the ambient effects subsystem through the console variable rts.AmbientFx (0 to 3).
 */
UENUM(BlueprintType)
enum class ERTSAmbientEffectsLevel : uint8
{
	Disabled UMETA(DisplayName="Disabled"),
	Low UMETA(DisplayName="Low"),
	Medium UMETA(DisplayName="Medium"),
	Maximum UMETA(DisplayName="Maximum")
};

inline FText global_GetAmbientEffectsLevelText(const ERTSAmbientEffectsLevel Level)
{
	switch (Level)
	{
	case ERTSAmbientEffectsLevel::Disabled:
		return FText::FromString(TEXT("Disabled"));
	case ERTSAmbientEffectsLevel::Low:
		return FText::FromString(TEXT("Low"));
	case ERTSAmbientEffectsLevel::Medium:
		return FText::FromString(TEXT("Medium"));
	case ERTSAmbientEffectsLevel::Maximum:
		return FText::FromString(TEXT("Maximum"));
	}
	return FText::FromString(TEXT("Maximum"));
}
