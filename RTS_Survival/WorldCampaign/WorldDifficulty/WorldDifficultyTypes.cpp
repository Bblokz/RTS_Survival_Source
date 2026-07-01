// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDifficulty/WorldDifficultyTypes.h"

float FRTSPerDifficultyMlt::GetMlt(const ERTSGameDifficulty GameDifficulty) const
{
	switch (GameDifficulty)
	{
	case ERTSGameDifficulty::NewToRTS:
		return NewToRTSMlt;
	case ERTSGameDifficulty::Hard:
		return HardMlt;
	case ERTSGameDifficulty::Brutal:
		return BrutalMlt;
	case ERTSGameDifficulty::Ironman:
		return IronmanMlt;
	case ERTSGameDifficulty::Normal:
	default:
		return NormalMlt;
	}
}

int32 FRTSPerDifficultyMlt::ApplyToPercentage(const int32 BasePercentage,
                                              const ERTSGameDifficulty GameDifficulty) const
{
	return FMath::RoundToInt(static_cast<float>(BasePercentage) * GetMlt(GameDifficulty));
}
