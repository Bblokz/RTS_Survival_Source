#include "W_StrengthEstimation.h"

#include "Components/RichTextBlock.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_StrengthEstimation::SetupStrengthEstimation(
	const FRTSStrengthEstimationRichTextMessage& StrengthEstimationMessage)
{
	const bool bHasValidEstimationsRichText = GetIsValidEstimationsRichText();
	const bool bHasValidTotalRichText = GetIsValidTotalRichText();

	if (not bHasValidEstimationsRichText || not bHasValidTotalRichText)
	{
		return;
	}

	FRTSStrengthEstimationRichTextMessage FormattedStrengthEstimationMessage = StrengthEstimationMessage;
	FormattedStrengthEstimationMessage.FormatRichTextMessages();

	M_EstimationsRichText->SetText(FormattedStrengthEstimationMessage.EstimationsRichText);
	M_TotalRichText->SetText(FormattedStrengthEstimationMessage.TotalRichText);
}

bool UW_StrengthEstimation::GetIsValidEstimationsRichText() const
{
	if (IsValid(M_EstimationsRichText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		FString::Printf(TEXT("%s: M_EstimationsRichText is invalid."), *GetNameSafe(this)));

	return false;
}

bool UW_StrengthEstimation::GetIsValidTotalRichText() const
{
	if (IsValid(M_TotalRichText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		FString::Printf(TEXT("%s: M_TotalRichText is invalid."), *GetNameSafe(this)));

	return false;
}
