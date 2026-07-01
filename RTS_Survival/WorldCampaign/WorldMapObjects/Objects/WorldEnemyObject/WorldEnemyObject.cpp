// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"

namespace
{
	const FText& GetEnemyObjectBaseDifficultyReasonText()
	{
		static const FText EnemyObjectBaseDifficultyReasonText =
			FText::FromString(TEXT("<Text_NewBad>Base Difficulty</>"));
		return EnemyObjectBaseDifficultyReasonText;
	}
}

void AWorldEnemyObject::InitializeForAnchorWithEnemyItem(AAnchorPoint* AnchorPoint, EMapEnemyItem EnemyItemType)
{
	AWorldMapObject::InitializeForAnchor(AnchorPoint);
	M_EnemyItemType = EnemyItemType;
}

void AWorldEnemyObject::SetPrimaryReward(const FPrimaryReward& PrimaryReward)
{
	M_PrimaryReward = PrimaryReward;
}

void AWorldEnemyObject::SetBaseDifficultyInfluenceReason(
	const FRTSStrengthEstimationInfluenceReason& InfluenceReason)
{
	M_BaseDifficultyInfluenceReason = InfluenceReason;
	RebuildDifficultyInfluenceReasons();
}

int32 AWorldEnemyObject::GetBaseDifficultyPercentage() const
{
	return M_BaseDifficultyInfluenceReason.InfluencePercent;
}

void AWorldEnemyObject::SetBaseDifficultyPercentage(const int32 DifficultyPercentage)
{
	M_BaseDifficultyInfluenceReason.ReasonText = GetEnemyObjectBaseDifficultyReasonText();
	M_BaseDifficultyInfluenceReason.InfluencePercent = DifficultyPercentage;
	RebuildDifficultyInfluenceReasons();
}

void AWorldEnemyObject::AddBaseDifficultyPercentage(const int32 AddedDifficultyPercentage)
{
	SetBaseDifficultyPercentage(GetBaseDifficultyPercentage() + AddedDifficultyPercentage);
}

void AWorldEnemyObject::ResetAuxiliaryDifficultyInfluenceReasons()
{
	M_AuxiliaryDifficultyInfluenceReasons.Reset();
	RebuildDifficultyInfluenceReasons();
}

void AWorldEnemyObject::AddAuxiliaryDifficultyInfluenceReason(
	const FRTSStrengthEstimationInfluenceReason& InfluenceReason)
{
	if (not InfluenceReason.GetHasInfluence())
	{
		return;
	}

	M_AuxiliaryDifficultyInfluenceReasons.Add(InfluenceReason);
	RebuildDifficultyInfluenceReasons();
}

void AWorldEnemyObject::RebuildDifficultyInfluenceReasons()
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

void AWorldEnemyObject::SetSecondaryObjectiveData(const EBonusObjective BonusObjective,
                                                  const FText& SecondaryObjectiveText,
                                                  const FSecondaryReward& SecondaryReward)
{
	M_BonusObjective = BonusObjective;
	M_MapItemUIData.SecondaryObjectiveText = SecondaryObjectiveText;
	M_SecondaryReward = SecondaryReward;
}
