// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"

namespace
{
	const FText& GetMissionObjectBaseDifficultyReasonText()
	{
		static const FText MissionObjectBaseDifficultyReasonText =
			FText::FromString(TEXT("<Text_NewBad>Base Difficulty</>"));
		return MissionObjectBaseDifficultyReasonText;
	}
}

void AWorldMissionObject::InitializeForAnchorWithMissionType(AAnchorPoint* AnchorPoint, EMapMission MissionType)
{
	AWorldMapObject::InitializeForAnchor(AnchorPoint);
	M_MissionType = MissionType;
}

void AWorldMissionObject::SetBaseDifficultyInfluenceReason(
	const FRTSStrengthEstimationInfluenceReason& InfluenceReason)
{
	M_BaseDifficultyInfluenceReason = InfluenceReason;
	RebuildDifficultyInfluenceReasons();
}

int32 AWorldMissionObject::GetBaseDifficultyPercentage() const
{
	return M_BaseDifficultyInfluenceReason.InfluencePercent;
}

void AWorldMissionObject::SetBaseDifficultyPercentage(const int32 DifficultyPercentage)
{
	M_BaseDifficultyInfluenceReason.ReasonText = GetMissionObjectBaseDifficultyReasonText();
	M_BaseDifficultyInfluenceReason.InfluencePercent = DifficultyPercentage;
	RebuildDifficultyInfluenceReasons();
}

void AWorldMissionObject::AddBaseDifficultyPercentage(const int32 AddedDifficultyPercentage)
{
	SetBaseDifficultyPercentage(GetBaseDifficultyPercentage() + AddedDifficultyPercentage);
}

void AWorldMissionObject::ResetAuxiliaryDifficultyInfluenceReasons()
{
	M_AuxiliaryDifficultyInfluenceReasons.Reset();
	RebuildDifficultyInfluenceReasons();
}

void AWorldMissionObject::AddAuxiliaryDifficultyInfluenceReason(
	const FRTSStrengthEstimationInfluenceReason& InfluenceReason)
{
	if (not InfluenceReason.GetHasInfluence())
	{
		return;
	}

	M_AuxiliaryDifficultyInfluenceReasons.Add(InfluenceReason);
	RebuildDifficultyInfluenceReasons();
}

void AWorldMissionObject::RebuildDifficultyInfluenceReasons()
{
	TArray<FRTSStrengthEstimationInfluenceReason> InfluenceReasons;
	if (M_BaseDifficultyInfluenceReason.GetHasInfluence())
	{
		InfluenceReasons.Add(M_BaseDifficultyInfluenceReason);
	}

	for (const FRTSStrengthEstimationInfluenceReason& AuxiliaryInfluenceReason : M_AuxiliaryDifficultyInfluenceReasons)
	{
		if (not AuxiliaryInfluenceReason.GetHasInfluence())
		{
			continue;
		}

		InfluenceReasons.Add(AuxiliaryInfluenceReason);
	}

	M_MapItemUIData.SetStrengthInfluenceReasons(InfluenceReasons);
}
