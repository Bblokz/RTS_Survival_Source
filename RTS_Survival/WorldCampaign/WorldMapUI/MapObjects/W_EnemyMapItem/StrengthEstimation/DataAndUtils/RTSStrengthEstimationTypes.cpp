#include "RTSStrengthEstimationTypes.h"

const TCHAR FRTSStrengthEstimationInfluenceReason::PositiveInfluenceSignOpeningTag[] = TEXT("<Text_NewBad>");
const TCHAR FRTSStrengthEstimationInfluenceReason::PositiveInfluenceValueOpeningTag[] = TEXT("<Text_Bad14>");
const TCHAR FRTSStrengthEstimationInfluenceReason::NegativeInfluenceSignOpeningTag[] = TEXT("<Text_NewGood>");
const TCHAR FRTSStrengthEstimationInfluenceReason::NegativeInfluenceValueOpeningTag[] = TEXT("<Text_Armor>");
const TCHAR FRTSStrengthEstimationInfluenceReason::PositiveInfluenceSignText[] = TEXT("+");
const TCHAR FRTSStrengthEstimationInfluenceReason::NegativeInfluenceSignText[] = TEXT("-");
const TCHAR FRTSStrengthEstimationInfluenceReason::RichTextClosingTag[] = TEXT("</>");


const TCHAR FRTSStrengthEstimationRichTextMessage::InfluenceReasonLineSeparator[] = TEXT("\n\n");
const TCHAR FRTSStrengthEstimationRichTextMessage::TotalTitleOpeningTag[] = TEXT("<Text_BadTitle>");
const TCHAR FRTSStrengthEstimationRichTextMessage::TotalPositiveValueOpeningTag[] = TEXT("<Text_NewBad>");
const TCHAR FRTSStrengthEstimationRichTextMessage::TotalNegativeValueOpeningTag[] = TEXT("<Text_NewGood>");
const TCHAR FRTSStrengthEstimationRichTextMessage::RichTextClosingTag[] = TEXT("</>");

FRTSStrengthEstimationInfluenceReason::FRTSStrengthEstimationInfluenceReason(
	const FText& InReasonText,
	const int32 InInfluencePercent)
	: ReasonText(InReasonText)
	, InfluencePercent(InInfluencePercent)
{
}

bool FRTSStrengthEstimationInfluenceReason::GetHasInfluence() const
{
	return InfluencePercent != 0;
}

bool FRTSStrengthEstimationInfluenceReason::GetIsNegativeInfluence() const
{
	return InfluencePercent < 0;
}

FText FRTSStrengthEstimationInfluenceReason::BuildRichTextLine() const
{
	if (not GetHasInfluence())
	{
		return FText::GetEmpty();
	}

	const int64 AbsoluteInfluencePercent = FMath::Abs(static_cast<int64>(InfluencePercent));

	FFormatNamedArguments RichTextArguments;
	RichTextArguments.Add(TEXT("SignOpeningTag"), FText::FromString(FString(GetSignOpeningTag())));
	RichTextArguments.Add(TEXT("SignText"), FText::FromString(FString(GetSignText())));
	RichTextArguments.Add(TEXT("ValueOpeningTag"), FText::FromString(FString(GetValueOpeningTag())));
	RichTextArguments.Add(TEXT("InfluencePercent"), FText::FromString(LexToString(AbsoluteInfluencePercent)));
	RichTextArguments.Add(TEXT("Reason"), ReasonText);

	return FText::Format(GetRichTextLineFormat(), RichTextArguments);
}

const FText& FRTSStrengthEstimationInfluenceReason::GetRichTextLineFormat()
{
	static const FText RichTextLineFormat = FText::FromString(
		FString::Printf(
			TEXT("{SignOpeningTag}{SignText}%s{ValueOpeningTag}{InfluencePercent}%% {Reason}%s"),
			RichTextClosingTag,
			RichTextClosingTag));

	return RichTextLineFormat;
}

const TCHAR* FRTSStrengthEstimationInfluenceReason::GetSignOpeningTag() const
{
	if (GetIsNegativeInfluence())
	{
		return NegativeInfluenceSignOpeningTag;
	}

	return PositiveInfluenceSignOpeningTag;
}

const TCHAR* FRTSStrengthEstimationInfluenceReason::GetValueOpeningTag() const
{
	if (GetIsNegativeInfluence())
	{
		return NegativeInfluenceValueOpeningTag;
	}

	return PositiveInfluenceValueOpeningTag;
}

const TCHAR* FRTSStrengthEstimationInfluenceReason::GetSignText() const
{
	if (GetIsNegativeInfluence())
	{
		return NegativeInfluenceSignText;
	}

	return PositiveInfluenceSignText;
}



void FRTSStrengthEstimationRichTextMessage::FormatRichTextMessages()
{
	EstimationsRichText = BuildEstimationsRichText();
	TotalRichText = BuildTotalRichText();
}

FText FRTSStrengthEstimationRichTextMessage::BuildEstimationsRichText() const
{
	TArray<FText> RichTextLines;
	RichTextLines.Reserve(InfluenceReasons.Num());

	for (const FRTSStrengthEstimationInfluenceReason& InfluenceReason : InfluenceReasons)
	{
		if (not InfluenceReason.GetHasInfluence())
		{
			continue;
		}

		RichTextLines.Add(InfluenceReason.BuildRichTextLine());
	}

	if (RichTextLines.Num() == 0)
	{
		return FText::GetEmpty();
	}

	return FText::Join(FText::FromString(FString(InfluenceReasonLineSeparator)), RichTextLines);
}

FText FRTSStrengthEstimationRichTextMessage::BuildTotalRichText() const
{
	const int32 TotalInfluencePercent = GetTotalInfluencePercent();

	FFormatNamedArguments RichTextArguments;
	RichTextArguments.Add(TEXT("TitleOpeningTag"), FText::FromString(FString(TotalTitleOpeningTag)));
	RichTextArguments.Add(TEXT("ValueOpeningTag"), FText::FromString(FString(GetTotalValueOpeningTag(TotalInfluencePercent))));
	RichTextArguments.Add(TEXT("TotalInfluencePercent"), FText::FromString(LexToString(TotalInfluencePercent)));

	return FText::Format(GetTotalRichTextFormat(), RichTextArguments);
}

int32 FRTSStrengthEstimationRichTextMessage::GetTotalInfluencePercent() const
{
	int32 TotalInfluencePercent = 0;

	for (const FRTSStrengthEstimationInfluenceReason& InfluenceReason : InfluenceReasons)
	{
		TotalInfluencePercent += InfluenceReason.InfluencePercent;
	}

	return TotalInfluencePercent;
}

const FText& FRTSStrengthEstimationRichTextMessage::GetTotalRichTextFormat()
{
	static const FText TotalRichTextFormat = FText::FromString(
		TEXT("{TitleOpeningTag}Total: </>{ValueOpeningTag}{TotalInfluencePercent}%</>"));

	return TotalRichTextFormat;
}

const TCHAR* FRTSStrengthEstimationRichTextMessage::GetTotalValueOpeningTag(const int32 TotalInfluencePercent)
{
	if (TotalInfluencePercent < 0)
	{
		return TotalNegativeValueOpeningTag;
	}

	return TotalPositiveValueOpeningTag;
}
