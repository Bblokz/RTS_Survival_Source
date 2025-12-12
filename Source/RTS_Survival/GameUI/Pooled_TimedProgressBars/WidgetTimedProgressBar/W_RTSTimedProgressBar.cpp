#include "W_RTSTimedProgressBar.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UW_RTSTimedProgressBar::UW_RTSTimedProgressBar(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UW_RTSTimedProgressBar::ActivateProgressBar(const float InitialRatio,
                                                 const bool bInUsePercentageText,
                                                 const ERTSProgressBarType InType, const bool bInUseText, const FString& InText)
{
	if (!ensure(M_ProgressBar))
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_RTSTimedProgressBar: ProgressBar not bound."));
		return;
	}
	if (!ensure(M_ProgressText))
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_RTSTimedProgressBar: ProgressText not bound."));
		return;
	}
	if(!ensure(M_RichProgressText))
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_RTSTimedProgressBar: RichProgressText not bound."));
		return;
	}

	const float Clamped = FMath::Clamp(InitialRatio, 0.0f, 1.0f);
	M_ProgressBar->SetPercent(Clamped);
	ApplyPercentText(Clamped, bInUsePercentageText);
	ApplyDescriptionText(bInUseText, InText);

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SetRenderOpacity(1.0f);
	bM_IsDormant = false;

	OnProgressBarTypeApplied(InType);
}

void UW_RTSTimedProgressBar::UpdateProgress(const float Ratio, const bool bInUsePercentageText)
{
	if (!ensure(M_ProgressBar))
	{
		return;
	}
	const float Clamped = FMath::Clamp(Ratio, 0.0f, 1.0f);
	M_ProgressBar->SetPercent(Clamped);

	ApplyPercentText(Clamped, bInUsePercentageText);
}

void UW_RTSTimedProgressBar::ApplyPercentText(const float Ratio, const bool bInUsePercentageText) const
{
	if (!ensure(M_ProgressText))
	{
		return;
	}

	if (bInUsePercentageText)
	{
		const int32 Percent = FMath::Clamp(FMath::RoundToInt(Ratio * 100.0f), 0, 100);
		M_ProgressText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Percent)));
		M_ProgressText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		M_ProgressText->SetText(FText::GetEmpty());
		M_ProgressText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UW_RTSTimedProgressBar::ApplyDescriptionText(const bool bInUseText, const FString& InText) const
{
	if (!ensure(M_RichProgressText))
	{
		return;
	}

	if (bInUseText && !InText.IsEmpty())
	{
		M_RichProgressText->SetText(FText::FromString(InText));
		M_RichProgressText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		M_RichProgressText->SetText(FText::GetEmpty());
		M_RichProgressText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UW_RTSTimedProgressBar::SetDormant(const bool bInDormant)
{
	bM_IsDormant = bInDormant;
	if (bM_IsDormant)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		SetRenderOpacity(0.0f);

		// Reset contents to a clean state for the next activation.
		if (M_ProgressBar)
		{
			M_ProgressBar->SetPercent(0.0f);
		}
		if (M_ProgressText)
		{
			M_ProgressText->SetText(FText::GetEmpty());
			M_ProgressText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
