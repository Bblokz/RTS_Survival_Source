#include "RTSStrengthEstimationTypes.h"

const TCHAR FWorldStrengthReason::PositiveStrengthSignOpeningTag[] = TEXT("<Text_NewBad>");
const TCHAR FWorldStrengthReason::PositiveStrengthValueOpeningTag[] = TEXT("<Text_Bad14>");
const TCHAR FWorldStrengthReason::NegativeStrengthSignOpeningTag[] = TEXT("<Text_NewGood>");
const TCHAR FWorldStrengthReason::NegativeStrengthValueOpeningTag[] = TEXT("<Text_Armor>");
const TCHAR FWorldStrengthReason::PositiveStrengthSignText[] = TEXT("+");
const TCHAR FWorldStrengthReason::NegativeStrengthSignText[] = TEXT("-");
const TCHAR FWorldStrengthReason::RichTextClosingTag[] = TEXT("</>");


const TCHAR FWorldStrengthReport::StrengthReasonLineSeparator[] = TEXT("\n\n");
const TCHAR FWorldStrengthReport::TotalTitleOpeningTag[] = TEXT("<Text_BadTitle>");
const TCHAR FWorldStrengthReport::TotalPositiveValueOpeningTag[] = TEXT("<Text_NewBad>");
const TCHAR FWorldStrengthReport::TotalNegativeValueOpeningTag[] = TEXT("<Text_NewGood>");
const TCHAR FWorldStrengthReport::RichTextClosingTag[] = TEXT("</>");

FWorldStrengthReason::FWorldStrengthReason(
	const FText& InStrengthReason,
	const int32 InStrengthPercent)
	: StrengthReason(InStrengthReason)
	, StrengthPercent(InStrengthPercent)
{
}

bool FWorldStrengthReason::GetHasStrength() const
{
	return StrengthPercent != 0;
}

bool FWorldStrengthReason::GetIsNegativeStrength() const
{
	return StrengthPercent < 0;
}

FText FWorldStrengthReason::BuildRichTextLine() const
{
	if (not GetHasStrength())
	{
		return FText::GetEmpty();
	}

	const int64 AbsoluteStrengthPercent = FMath::Abs(static_cast<int64>(StrengthPercent));

	FFormatNamedArguments RichTextArguments;
	RichTextArguments.Add(TEXT("SignOpeningTag"), FText::FromString(FString(GetSignOpeningTag())));
	RichTextArguments.Add(TEXT("SignText"), FText::FromString(FString(GetSignText())));
	RichTextArguments.Add(TEXT("ValueOpeningTag"), FText::FromString(FString(GetValueOpeningTag())));
	RichTextArguments.Add(TEXT("StrengthPercent"), FText::FromString(LexToString(AbsoluteStrengthPercent)));
	RichTextArguments.Add(TEXT("Reason"), StrengthReason);

	return FText::Format(GetRichTextLineFormat(), RichTextArguments);
}

const FText& FWorldStrengthReason::GetRichTextLineFormat()
{
	static const FText RichTextLineFormat = FText::FromString(
		FString::Printf(
			TEXT("{SignOpeningTag}{SignText}%s{ValueOpeningTag}{StrengthPercent}%% {Reason}%s"),
			RichTextClosingTag,
			RichTextClosingTag));

	return RichTextLineFormat;
}

const TCHAR* FWorldStrengthReason::GetSignOpeningTag() const
{
	if (GetIsNegativeStrength())
	{
		return NegativeStrengthSignOpeningTag;
	}

	return PositiveStrengthSignOpeningTag;
}

const TCHAR* FWorldStrengthReason::GetValueOpeningTag() const
{
	if (GetIsNegativeStrength())
	{
		return NegativeStrengthValueOpeningTag;
	}

	return PositiveStrengthValueOpeningTag;
}

const TCHAR* FWorldStrengthReason::GetSignText() const
{
	if (GetIsNegativeStrength())
	{
		return NegativeStrengthSignText;
	}

	return PositiveStrengthSignText;
}



void FWorldStrengthReport::FormatRichTextMessages()
{
	/*
	 * Rich text fields are derived from the reason arrays. Keep all four cached strings refreshed together so the
	 * hover widget can copy the report and bind each category without recalculating totals in the widget layer.
	 */
	FortificationStrengthRichText = BuildRichTextForStrengthType(EWorldStrengthTypes::FortificationStrength);
	FieldDivisionsRichText = BuildRichTextForStrengthType(EWorldStrengthTypes::FieldDivisions);
	StrategicSupportRichText = BuildRichTextForStrengthType(EWorldStrengthTypes::StrategicSupport);
	TotalRichText = BuildTotalRichText();
}

void FWorldStrengthReport::Reset()
{
	FortificationStrengthReasons.Reset();
	FieldDivisionReasons.Reset();
	StrategicSupportReasons.Reset();
	FormatRichTextMessages();
}

void FWorldStrengthReport::SetStrengthReasons(
	const EWorldStrengthTypes StrengthType,
	const TArray<FWorldStrengthReason>& InStrengthReasons)
{
	switch (StrengthType)
	{
	case EWorldStrengthTypes::FortificationStrength:
		FortificationStrengthReasons = InStrengthReasons;
		break;
	case EWorldStrengthTypes::FieldDivisions:
		FieldDivisionReasons = InStrengthReasons;
		break;
	case EWorldStrengthTypes::StrategicSupport:
		StrategicSupportReasons = InStrengthReasons;
		break;
	case EWorldStrengthTypes::None:
	default:
		break;
	}

	FormatRichTextMessages();
}

void FWorldStrengthReport::AddStrengthReason(
	const EWorldStrengthTypes StrengthType,
	const FWorldStrengthReason& StrengthReason)
{
	if (not StrengthReason.GetHasStrength())
	{
		return;
	}

	switch (StrengthType)
	{
	case EWorldStrengthTypes::FortificationStrength:
		FortificationStrengthReasons.Add(StrengthReason);
		break;
	case EWorldStrengthTypes::FieldDivisions:
		FieldDivisionReasons.Add(StrengthReason);
		break;
	case EWorldStrengthTypes::StrategicSupport:
		StrategicSupportReasons.Add(StrengthReason);
		break;
	case EWorldStrengthTypes::None:
	default:
		return;
	}

	FormatRichTextMessages();
}

const TArray<FWorldStrengthReason>& FWorldStrengthReport::GetStrengthReasons(
	const EWorldStrengthTypes StrengthType) const
{
	switch (StrengthType)
	{
	case EWorldStrengthTypes::FortificationStrength:
		return FortificationStrengthReasons;
	case EWorldStrengthTypes::FieldDivisions:
		return FieldDivisionReasons;
	case EWorldStrengthTypes::StrategicSupport:
		return StrategicSupportReasons;
	case EWorldStrengthTypes::None:
	default:
		return GetEmptyStrengthReasons();
	}
}

FText FWorldStrengthReport::BuildRichTextForStrengthType(
	const EWorldStrengthTypes StrengthType) const
{
	const TArray<FWorldStrengthReason>& StrengthReasons = GetStrengthReasons(StrengthType);
	TArray<FText> RichTextLines;
	RichTextLines.Reserve(StrengthReasons.Num());

	for (const FWorldStrengthReason& StrengthReason : StrengthReasons)
	{
		if (not StrengthReason.GetHasStrength())
		{
			continue;
		}

		RichTextLines.Add(StrengthReason.BuildRichTextLine());
	}

	if (RichTextLines.Num() == 0)
	{
		return FText::GetEmpty();
	}

	return FText::Join(FText::FromString(FString(StrengthReasonLineSeparator)), RichTextLines);
}

FText FWorldStrengthReport::BuildTotalRichText() const
{
	const int32 TotalStrengthPercent = GetTotalStrengthPercent();

	FFormatNamedArguments RichTextArguments;
	RichTextArguments.Add(TEXT("TitleOpeningTag"), FText::FromString(FString(TotalTitleOpeningTag)));
	RichTextArguments.Add(TEXT("ValueOpeningTag"), FText::FromString(FString(GetTotalValueOpeningTag(TotalStrengthPercent))));
	RichTextArguments.Add(TEXT("TotalStrengthPercent"), FText::FromString(LexToString(TotalStrengthPercent)));

	return FText::Format(GetTotalRichTextFormat(), RichTextArguments);
}

int32 FWorldStrengthReport::GetTotalStrengthPercent() const
{
	return GetStrengthPercentForStrengthType(EWorldStrengthTypes::FortificationStrength)
		+ GetStrengthPercentForStrengthType(EWorldStrengthTypes::FieldDivisions)
		+ GetStrengthPercentForStrengthType(EWorldStrengthTypes::StrategicSupport);
}

int32 FWorldStrengthReport::GetStrengthPercentForStrengthType(
	const EWorldStrengthTypes StrengthType) const
{
	const TArray<FWorldStrengthReason>& StrengthReasons = GetStrengthReasons(StrengthType);
	int32 StrengthPercent = 0;

	for (const FWorldStrengthReason& StrengthReason : StrengthReasons)
	{
		StrengthPercent += StrengthReason.StrengthPercent;
	}

	return StrengthPercent;
}

const FText& FWorldStrengthReport::GetTotalRichTextFormat()
{
	static const FText TotalRichTextFormat = FText::FromString(
		TEXT("{TitleOpeningTag}Total: </>{ValueOpeningTag}{TotalStrengthPercent}%</>"));

	return TotalRichTextFormat;
}

const TCHAR* FWorldStrengthReport::GetTotalValueOpeningTag(const int32 TotalStrengthPercent)
{
	if (TotalStrengthPercent < 0)
	{
		return TotalNegativeValueOpeningTag;
	}

	return TotalPositiveValueOpeningTag;
}

const TArray<FWorldStrengthReason>& FWorldStrengthReport::GetEmptyStrengthReasons()
{
	static const TArray<FWorldStrengthReason> EmptyStrengthReasons;
	return EmptyStrengthReasons;
}
