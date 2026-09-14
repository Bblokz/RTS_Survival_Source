#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconTypes.h"
#include "BehaviourVerticalIconSettings.generated.h"

UENUM(BlueprintType)
enum class EBehaviourRepeatedVerticalIconStrategy : uint8
{
	PerAmountRepeats,
	InfiniteRepeats
};

/** @brief Selects the pooled artwork and actor-relative presentation for behaviour feedback. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FBehaviourIconSettings
{
	GENERATED_BODY()

	// Takes precedence over animated text, including when the selected icon is None or cannot be displayed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Icons")
	bool bUseIcons = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Icons", meta=(EditCondition="bUseIcons"))
	ERTSVerticalAnimatedIcon IconType = ERTSVerticalAnimatedIcon::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Icons", meta=(EditCondition="bUseIcons"))
	FVector LocalOffset = FVector(0.0, 0.0, 120.0);

	// Otherwise the catalog supplies the visible duration, fade duration and world-Z travel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Icons", meta=(EditCondition="bUseIcons"))
	bool bOverrideAnimationSettings = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Icons",
		meta=(EditCondition="bUseIcons && bOverrideAnimationSettings", EditConditionHides))
	FRTSVerticalAnimIconSettings AnimationSettings;
};

/** @brief Configures optional repeated icons without requiring the behaviour's gameplay OnTick callback. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FRepeatedBehaviourIconSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Icons")
	FBehaviourIconSettings IconSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Icons")
	EBehaviourRepeatedVerticalIconStrategy RepeatStrategy = EBehaviourRepeatedVerticalIconStrategy::PerAmountRepeats;

	// Includes the initial display; one or less displays once without scheduling repeats.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Icons", meta=(ClampMin="1"))
	int32 AmountRepeats = 1;

	// Repeats use the behaviour component's existing tick cadence, with no catch-up burst after a long frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Icons", meta=(ClampMin="0.0", Units="s"))
	float RepeatInterval = 4.0f;
};
