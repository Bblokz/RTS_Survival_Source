#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/AOEBehaviourComponent.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/RTSVerticalAnimatedText/RTSVerticalAnimatedText.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/Constants/AoeBehaviourConstants.h"


#include "BehaviourVerticalTextSettings.generated.h"

UENUM(BlueprintType)
enum class EBehaviourRepeatedVerticalTextStrategy : uint8
{
	PerAmountRepeats,
	InfiniteRepeats
};

/**
 * @brief Text layout settings for the AOE behaviour component.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FBehaviourTextSettings
{
	GENERATED_BODY()

	/** When enabled, pooled text is shown above affected units each tick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	bool bUseText = false;

	/** Text shown above affected units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	FString TextOnSubjects = TEXT("<Text_Exp>Aura</>");

	/** Local offset from the target actor root component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	FVector TextOffset = FVector(0.f, 0.f, AOEBehaviourComponentConstants::DefaultTextOffsetZ);

	/** When true the text will auto-wrap at InWrapAt (set by the component). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	bool bAutoWrap = true;

	/** Width in px when auto-wrap is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	float InWrapAt = AOEBehaviourComponentConstants::DefaultWrapAt;

	/** Text justification for the rich text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	TEnumAsByte<ETextJustify::Type> InJustification = ETextJustify::Center;

	/** Animation timings and vertical motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	FRTSVerticalAnimTextSettings InSettings;
};

USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FRepeatedBehaviourTextSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	EBehaviourRepeatedVerticalTextStrategy RepeatStrategy = EBehaviourRepeatedVerticalTextStrategy::PerAmountRepeats;

	// if set to one or lower it will not fire a timer but instantly create the text and that is it.
	// Note that if this is set to more than one the behaviour should also be set to tick so it can calculate when to show the text again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	int32 AmountRepeats = 1;

	// How long between repeats of vertical text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	float RepeatInterval = 4.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animated Text")
	FBehaviourTextSettings TextSettings;
};
