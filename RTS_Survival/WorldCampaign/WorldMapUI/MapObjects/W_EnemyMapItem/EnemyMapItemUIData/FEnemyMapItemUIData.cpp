#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_EnemyMapItem/EnemyMapItemUIData/FEnemyMapItemUIData.h"

namespace
{
	const TCHAR* GetStrengthPercentageValueOpeningTag(const int32 StrengthPercentage)
	{
		if (StrengthPercentage < 0)
		{
			return FWorldStrengthReport::TotalNegativeValueOpeningTag;
		}

		return FWorldStrengthReport::TotalPositiveValueOpeningTag;
	}
}

void FEnemyOrMissionMapItemUIData::ResetStrengthEstimation()
{
	StrengthEstimation.Reset();
	RefreshStrengthPercentageText();
}

void FEnemyOrMissionMapItemUIData::SetStrengthEstimation(
	const FWorldStrengthReport& StrengthEstimationMessage)
{
	StrengthEstimation = StrengthEstimationMessage;
	RefreshStrengthPercentageText();
}

void FEnemyOrMissionMapItemUIData::SetStrengthReasons(
	const TArray<FWorldStrengthReason>& StrengthReasons)
{
	SetStrengthReasons(EWorldStrengthTypes::FortificationStrength, StrengthReasons);
}

void FEnemyOrMissionMapItemUIData::SetStrengthReasons(
	const EWorldStrengthTypes StrengthType,
	const TArray<FWorldStrengthReason>& StrengthReasons)
{
	StrengthEstimation.SetStrengthReasons(StrengthType, StrengthReasons);
	RefreshStrengthPercentageText();
}

void FEnemyOrMissionMapItemUIData::AddStrengthReason(
	const FWorldStrengthReason& StrengthReason)
{
	AddStrengthReason(EWorldStrengthTypes::FortificationStrength, StrengthReason);
}

void FEnemyOrMissionMapItemUIData::AddStrengthReason(
	const EWorldStrengthTypes StrengthType,
	const FWorldStrengthReason& StrengthReason)
{
	StrengthEstimation.AddStrengthReason(StrengthType, StrengthReason);
	RefreshStrengthPercentageText();
}

void FEnemyOrMissionMapItemUIData::RefreshStrengthPercentageText()
{
	const int32 StrengthPercentage = StrengthEstimation.GetTotalStrengthPercent();
	StrengthPercentageText = FText::FromString(
		FString::Printf(
			TEXT("%s%d%%%s"),
			GetStrengthPercentageValueOpeningTag(StrengthPercentage),
			StrengthPercentage,
			FWorldStrengthReport::RichTextClosingTag));

	StrengthEstimation.FormatRichTextMessages();
}
