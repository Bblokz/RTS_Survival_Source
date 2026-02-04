#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/EnemyWorldPersonality.h/EnemyWorldPersonality.h"

#include "GameInstCampaignGenerationSettings.generated.h"


USTRUCT(BlueprintType)
struct FCampaignGenerationSettings
{
	GENERATED_BODY()
	// Whether a new campaign needs to be generated or we are loading a saved one.
	UPROPERTY(BlueprintReadOnly)
	bool bNeedsToGenerateCampaign = true;

	UPROPERTY(BlueprintReadOnly)
	int32 GenerationSeed = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bUsesExtraDifficultyPercentage = false;
	
	UPROPERTY(BlueprintReadOnly)
	EEnemyWorldPersonality EnemyWorldPersonality = EEnemyWorldPersonality::Balanced;
	
};