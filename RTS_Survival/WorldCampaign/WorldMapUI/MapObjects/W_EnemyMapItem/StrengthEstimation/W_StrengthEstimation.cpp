#include "W_StrengthEstimation.h"

#include "Components/RichTextBlock.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_StrengthEstimation::SetupStrengthEstimation(
	const FWorldStrengthReport& StrengthEstimationMessage)
{
	const bool bHasValidFortificationsRichText = GetIsValidFortificationsRichText();
	const bool bHasValidFieldDivisionsRichText = GetIsValidFieldDivisionsRichText();
	const bool bHasValidStrategicSupportRichText = GetIsValidStrategicSupportRichText();
	const bool bHasValidTotalRichText = GetIsValidTotalRichText();

	if (not bHasValidFortificationsRichText
		|| not bHasValidFieldDivisionsRichText
		|| not bHasValidStrategicSupportRichText
		|| not bHasValidTotalRichText)
	{
		return;
	}

	FWorldStrengthReport FormattedStrengthEstimationMessage = StrengthEstimationMessage;
	FormattedStrengthEstimationMessage.FormatRichTextMessages();

	M_FortificationsRichText->SetText(FormattedStrengthEstimationMessage.FortificationStrengthRichText);
	M_FieldDivisionsRichText->SetText(FormattedStrengthEstimationMessage.FieldDivisionsRichText);
	M_StrategicSupportRichText->SetText(FormattedStrengthEstimationMessage.StrategicSupportRichText);
	M_TotalRichText->SetText(FormattedStrengthEstimationMessage.TotalRichText);
}

void UW_StrengthEstimation::ShowVisibleAnimation()
{
	BP_ShowVisibleAnimation();
}

bool UW_StrengthEstimation::GetIsValidFortificationsRichText() const
{
	if (IsValid(M_FortificationsRichText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		FString::Printf(TEXT("%s: M_FortificationsRichText is invalid."), *GetNameSafe(this)));

	return false;
}

bool UW_StrengthEstimation::GetIsValidFieldDivisionsRichText() const
{
	if (IsValid(M_FieldDivisionsRichText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		FString::Printf(TEXT("%s: M_FieldDivisionsRichText is invalid."), *GetNameSafe(this)));

	return false;
}

bool UW_StrengthEstimation::GetIsValidStrategicSupportRichText() const
{
	if (IsValid(M_StrategicSupportRichText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		FString::Printf(TEXT("%s: M_StrategicSupportRichText is invalid."), *GetNameSafe(this)));

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
