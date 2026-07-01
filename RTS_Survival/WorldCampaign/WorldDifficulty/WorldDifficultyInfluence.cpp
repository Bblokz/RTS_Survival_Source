// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDifficulty/WorldDifficultyInfluence.h"

int32 FWorldDifficultyInfluenceSettings::GetDifficultyPercent(const ERTSGameDifficulty GameDifficulty) const
{
	return PerDifficultyMlt.ApplyToPercentage(BasePercentage, GameDifficulty);
}

FRTSStrengthEstimationInfluenceReason FWorldDifficultyInfluenceSettings::BuildInfluenceReason(
	const ERTSGameDifficulty GameDifficulty) const
{
	return FRTSStrengthEstimationInfluenceReason(ReasonText, GetDifficultyPercent(GameDifficulty));
}

UWorldDifficultyInfluence::UWorldDifficultyInfluence()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UWorldDifficultyInfluence::GetDifficultyInfluencePercent(const ERTSGameDifficulty GameDifficulty) const
{
	return M_Settings.GetDifficultyPercent(GameDifficulty);
}

FRTSStrengthEstimationInfluenceReason UWorldDifficultyInfluence::BuildInfluenceReason(
	const ERTSGameDifficulty GameDifficulty) const
{
	return M_Settings.BuildInfluenceReason(GameDifficulty);
}
