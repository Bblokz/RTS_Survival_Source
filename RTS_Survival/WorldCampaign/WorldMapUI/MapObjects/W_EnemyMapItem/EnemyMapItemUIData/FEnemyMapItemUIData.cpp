#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_EnemyMapItem/EnemyMapItemUIData/FEnemyMapItemUIData.h"

namespace
{
	const TCHAR* GetStrengthPercentageValueOpeningTag(const int32 StrengthPercentage)
	{
		if (StrengthPercentage < 0)
		{
			return FRTSStrengthEstimationRichTextMessage::TotalNegativeValueOpeningTag;
		}

		return FRTSStrengthEstimationRichTextMessage::TotalPositiveValueOpeningTag;
	}
}

void FEnemyOrMissionMapItemUIData::ResetStrengthEstimation()
{
	StrengthEstimation.InfluenceReasons.Reset();
	RefreshStrengthPercentageText();
}

void FEnemyOrMissionMapItemUIData::SetStrengthInfluenceReasons(
	const TArray<FRTSStrengthEstimationInfluenceReason>& InfluenceReasons)
{
	StrengthEstimation.InfluenceReasons = InfluenceReasons;
	RefreshStrengthPercentageText();
}

void FEnemyOrMissionMapItemUIData::AddStrengthInfluenceReason(
	const FRTSStrengthEstimationInfluenceReason& InfluenceReason)
{
	if (not InfluenceReason.GetHasInfluence())
	{
		return;
	}

	StrengthEstimation.InfluenceReasons.Add(InfluenceReason);
	RefreshStrengthPercentageText();
}

void FEnemyOrMissionMapItemUIData::RefreshStrengthPercentageText()
{
	const int32 StrengthPercentage = StrengthEstimation.GetTotalInfluencePercent();
	StrengthPercentageText = FText::FromString(
		FString::Printf(
			TEXT("%s%d%%%s"),
			GetStrengthPercentageValueOpeningTag(StrengthPercentage),
			StrengthPercentage,
			FRTSStrengthEstimationRichTextMessage::RichTextClosingTag));

	StrengthEstimation.FormatRichTextMessages();
}
