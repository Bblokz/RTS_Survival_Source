// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "WorldDifficultyTypes.generated.h"

/**
 * @brief Designer-authored multiplier set used to scale world difficulty percentages by selected game difficulty.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FRTSPerDifficultyMlt
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Difficulty")
	float NewToRTSMlt = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Difficulty")
	float NormalMlt = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Difficulty")
	float HardMlt = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Difficulty")
	float BrutalMlt = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Difficulty")
	float IronmanMlt = 1.f;

	float GetMlt(ERTSGameDifficulty GameDifficulty) const;
	int32 ApplyToPercentage(int32 BasePercentage, ERTSGameDifficulty GameDifficulty) const;
};
