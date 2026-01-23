// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTSGameDifficultySettings.generated.h"

/**
 * @brief Exposes difficulty tuning values for UI-driven selection before missions start.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Difficulty Settings"))
class RTS_SURVIVAL_API URTSGameDifficultySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	UPROPERTY(Config, EditAnywhere, Category="Difficulty")
	TMap<ERTSGameDifficulty, int32> DifficultyPercentageByLevel;
};
