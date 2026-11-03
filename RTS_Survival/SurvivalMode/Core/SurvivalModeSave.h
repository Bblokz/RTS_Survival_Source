// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"

#include "SurvivalModeSave.generated.h"

/**
 * @brief Best survival time per map and difficulty, keyed "<MapName>|<Difficulty>".
 */
UCLASS()
class RTS_SURVIVAL_API USurvivalModeSave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* SlotName = TEXT("SurvivalModeRecords");

	UPROPERTY(SaveGame)
	TMap<FString, float> BestSecondsPerKey;

	UPROPERTY(SaveGame)
	TMap<FString, int32> BestWavesPerKey;
};
