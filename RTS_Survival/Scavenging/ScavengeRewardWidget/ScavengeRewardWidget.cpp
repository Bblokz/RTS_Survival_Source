#include "ScavengeRewardWidget.h"
#include "Components/RichTextBlock.h"
#include "TimerManager.h"

void UScavengeRewardWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Initialize variables
    FadeDuration = 0.0f;
    CurrentFadeTime = 0.0f;
}

void UScavengeRewardWidget::SetRewardText(const FText& InText)
{
    if (RewardTextBlock)
    {
        RewardTextBlock->SetText(InText);
    }
}

void UScavengeRewardWidget::StartFadeOut(float InFadeDuration)
{
    FadeDuration = InFadeDuration;
    CurrentFadeTime = 0.0f;

    // Start a timer to update fade out every frame
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(FadeOutTimerHandle, this, &UScavengeRewardWidget::UpdateFadeOut, 0.01f, true);
    }
}

void UScavengeRewardWidget::UpdateFadeOut()
{
    CurrentFadeTime += 0.01f;
    float Alpha = 1.0f - (CurrentFadeTime / FadeDuration);

    // Update opacity of the widget
    if (RewardTextBlock)
    {
        RewardTextBlock->SetRenderOpacity(Alpha);
    }

    if (CurrentFadeTime >= FadeDuration)
    {
        // Stop the timer
        GetWorld()->GetTimerManager().ClearTimer(FadeOutTimerHandle);
    }
}
