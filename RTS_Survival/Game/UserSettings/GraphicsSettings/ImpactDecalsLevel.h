#pragma once

#include "CoreMinimal.h"

#include "ImpactDecalsLevel.generated.h"

/**
 * @brief How many shell craters (impact decals on the landscape) stay on the battlefield at once; the oldest makes way
 * for a new one. Off spawns none (for very cheap hardware).
 * The count is handed to ULandscapeImpactDecalSubsystem through the console variable rts.ImpactDecals.
 */
UENUM(BlueprintType)
enum class ERTSImpactDecalsLevel : uint8
{
	Off UMETA(DisplayName="Off"),
	Few UMETA(DisplayName="50"),
	Some UMETA(DisplayName="100"),
	Many UMETA(DisplayName="200"),
	Maximum UMETA(DisplayName="400")
};

inline int32 global_GetImpactDecalsCount(const ERTSImpactDecalsLevel Level)
{
	constexpr int32 FewDecals = 50;
	constexpr int32 SomeDecals = 100;
	constexpr int32 ManyDecals = 200;
	constexpr int32 MaximumDecals = 400;
	switch (Level)
	{
	case ERTSImpactDecalsLevel::Off:
		return 0;
	case ERTSImpactDecalsLevel::Few:
		return FewDecals;
	case ERTSImpactDecalsLevel::Some:
		return SomeDecals;
	case ERTSImpactDecalsLevel::Many:
		return ManyDecals;
	case ERTSImpactDecalsLevel::Maximum:
		return MaximumDecals;
	}
	return SomeDecals;
}

inline FText global_GetImpactDecalsText(const ERTSImpactDecalsLevel Level)
{
	if (Level == ERTSImpactDecalsLevel::Off)
	{
		return FText::FromString(TEXT("Off"));
	}
	return FText::AsNumber(global_GetImpactDecalsCount(Level));
}
