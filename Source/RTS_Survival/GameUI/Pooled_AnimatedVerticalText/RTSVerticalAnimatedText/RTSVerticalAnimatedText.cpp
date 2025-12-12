#include "RTSVerticalAnimatedText.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UW_RTSVerticalAnimatedText::UW_RTSVerticalAnimatedText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

bool UW_RTSVerticalAnimatedText::GetIsValidRichText() const
{
	if (IsValid(TextBlock_RichText))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("UW_RTSVerticalAnimatedText::TextBlock_RichText is not bound."));
	return false;
}

void UW_RTSVerticalAnimatedText::ActivateAnimatedText(const FString& InText,
                                                      const bool bInAutoWrap,
                                                      const float InWrapAt,
                                                      const TEnumAsByte<ETextJustify::Type> InJustification,
                                                      const FRTSVerticalAnimTextSettings& InSettings)
{
	if (not GetIsValidRichText())
	{
		return;
	}

	M_LastAnimSettings = InSettings;

	TextBlock_RichText->SetText(FText::FromString(InText));
	TextBlock_RichText->SetAutoWrapText(bInAutoWrap);
	TextBlock_RichText->SetWrapTextAt(InWrapAt);
	TextBlock_RichText->SetJustification(static_cast<ETextJustify::Type>(InJustification.GetValue()));

	TextBlock_RichText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	InvalidateLayoutAndVolatility();

	// IMPORTANT: we animate by moving the component in world space, not the widget in screen space.
	SetRenderTranslation(FVector2D::ZeroVector);
	SetRenderOpacity(1.0f);

	bM_IsDormant = false;

	if (UWorld* World = GetWorld())
	{
		M_ActivatedAtTimeSeconds = World->GetTimeSeconds();
	}
	else
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_RTSVerticalAnimatedText::ActivateAnimatedText - GetWorld() returned null."));
		M_ActivatedAtTimeSeconds = 0.0f;
	}
}



void UW_RTSVerticalAnimatedText::ApplyInitialScreenOffset(const FVector2D InStartTranslation)
{
	M_StartRenderTranslation = InStartTranslation;
	SetRenderTranslation(M_StartRenderTranslation);
}

void UW_RTSVerticalAnimatedText::SetDormant(const bool bInDormant)
{
	bM_IsDormant = bInDormant;
	if (bM_IsDormant)
	{
		// Reset visuals so the next activation starts clean.
		SetRenderOpacity(0.0f);
		SetRenderTranslation(M_StartRenderTranslation);
	}
}
