// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSGameDifficultySettings.h"

FName URTSGameDifficultySettings::GetCategoryName() const
{
	return TEXT("Game");
}

FName URTSGameDifficultySettings::GetSectionName() const
{
	return TEXT("RTS Difficulty Settings");
}
