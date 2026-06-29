// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_WorldGenerationLoadingScreen.h"

#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

void UW_WorldGenerationLoadingScreen::SetGenerationProgress(const float ProgressPercent, const FString& DescriptionText)
{
	const float ClampedProgress = FMath::Clamp(ProgressPercent, 0.f, 100.f);
	if (IsValid(Progressbar))
	{
		constexpr float PercentToNormalized = 0.01f;
		Progressbar->SetPercent(ClampedProgress * PercentToNormalized);
	}

	if (IsValid(StepDescription))
	{
		StepDescription->SetText(FRTSRichTextConverter::MakeRTSRichText(DescriptionText, ERTSRichText::Text_Armor));
	}

	if (IsValid(Percentage))
	{
		const FString PercentageText = FString::Printf(TEXT("%.0f%%"), ClampedProgress);
		Percentage->SetText(FRTSRichTextConverter::MakeRTSRichText(PercentageText, ERTSRichText::Text_Armor));
	}
}
