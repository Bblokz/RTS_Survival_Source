#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/EnemyWorldPersonality.h/EnemyWorldPersonality.h"
#include "RTSGameInstancePIEDeveloperSettings.generated.h"

/**
 * @brief Provides configurable PIE-only startup values for world generation and difficulty.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS PIE Startup"))
class RTS_SURVIVAL_API URTSGameInstancePIEDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSGameInstancePIEDeveloperSettings();

	// Enables loading the overrides when the game instance is created for a PIE world.
	UPROPERTY(Config, EditAnywhere, Category="PIE Startup")
	bool bApplyPIEStartupOverrides = true;

	UPROPERTY(Config, EditAnywhere, Category="PIE Startup", meta=(ClampMin="0"))
	int32 CampaignGenerationSeed = 2323402;

	UPROPERTY(Config, EditAnywhere, Category="PIE Startup")
	EEnemyWorldPersonality EnemyWorldPersonality = EEnemyWorldPersonality::Balanced;

	UPROPERTY(Config, EditAnywhere, Category="PIE Startup")
	ERTSGameDifficulty DifficultyLevel = ERTSGameDifficulty::Hard;

	UPROPERTY(Config, EditAnywhere, Category="PIE Startup", meta=(ClampMin="1"))
	int32 DifficultyPercentage = 150;
};
