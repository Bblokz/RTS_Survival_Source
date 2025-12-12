#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "ScavengeRewardWidget.generated.h"

/**
 * @class UScavengeRewardWidget
 *
 * @brief Widget to display the resources gained from scavenging.
 * It will fade out over a specified duration.
 */
UCLASS()
class RTS_SURVIVAL_API UScavengeRewardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Sets the text to display */
    UFUNCTION(BlueprintCallable, Category = "Scavenge Reward")
    void SetRewardText(const FText& InText);

    /** Starts the fade-out of the widget */
    UFUNCTION(BlueprintCallable, Category = "Scavenge Reward")
    void StartFadeOut(float InFadeDuration);

protected:
    virtual void NativeConstruct() override;

    /** Rich Text Block to display the reward text */
    UPROPERTY(meta = (BindWidget))
    URichTextBlock* RewardTextBlock;

private:
    /** Timer handle for fade out */
    FTimerHandle FadeOutTimerHandle;

    /** Duration over which the widget fades out */
    float FadeDuration;

    /** Function to update opacity during fade out */
    void UpdateFadeOut();

    /** Current fade out time */
    float CurrentFadeTime;
};
