#pragma once

#include "CoreMinimal.h"
#include "AnimatedIconTypes.generated.h"

UENUM(BlueprintType)
enum class ERTSVerticalAnimatedIcon : uint8
{
	None,
	CommanderBoost,
	RangeBoost,
	Healing
};

/** @brief Callers can override an icon's default lifetime without changing its shared artwork. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FRTSVerticalAnimIconSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Icons", meta=(ClampMin="0.0", Units="s"))
	float VisibleDuration = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Icons", meta=(ClampMin="0.0", Units="s"))
	float FadeOutDuration = 0.5f;

	// World-Z travel over both the visible and fade periods; this is not a screen-pixel offset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Icons", meta=(Units="cm"))
	float DeltaZ = 30.0f;

	bool IsUsable() const;

	/**
	 * @brief Shares one time basis for movement and fading, including zero-length fade periods.
	 * @param ElapsedSeconds World game time since activation.
	 * @param OutWorldOffsetZ Accumulated upward travel in world units.
	 * @param OutOpacity Widget opacity, excluding the texture and tint alpha.
	 * @return Whether the icon is still within its lifetime.
	 */
	bool Evaluate(const double ElapsedSeconds, float& OutWorldOffsetZ, float& OutOpacity) const;
};
