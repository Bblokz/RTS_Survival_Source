#pragma once

#include "CoreMinimal.h"

#include "GameDifficulty.generated.h"


UENUM(BlueprintType)
enum class ERTSGameDifficulty : uint8
{
	NewToRTS,
	Normal,
	Hard,
	Brutal,
	Ironman
};

USTRUCT(Blueprintable)
struct FRTSGameDifficulty
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	int32 DifficultyPercentage  =0;

	UPROPERTY(BlueprintReadWrite)
	ERTSGameDifficulty DifficultyLevel = ERTSGameDifficulty::Normal;
	
	UPROPERTY(BlueprintReadWrite)
	bool bIsInitialized = false;
};