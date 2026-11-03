// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "UObject/Object.h"

#include "SurvivalStrategicAICoordinator.generated.h"

class AEnemyController;
class ASurvivalModeDirector;
class UEnemyStrategicAIComponent;
class USurvivalWaveSettings;
struct FSurvivalWaveDifficultyProfile;

/**
 * @brief Configures the map's enemy strategic AI for survival so it coexists with the wave director: difficulty
 * on the blackboard, start of thinking, direct control on/off, unit-pick limits and the Soviet bases' training
 * income over time, and (with the integration hook) the survival decision tree.
 * Units in wave formations are never touched here; the strategic AI only ever commands idle blackboard units.
 * @note Created and driven by USurvivalWaveDirectorComponent; not placed in levels.
 */
UCLASS()
class RTS_SURVIVAL_API USurvivalStrategicAICoordinator : public UObject
{
	GENERATED_BODY()

public:
	/** @brief Caches the director and settings; nothing is applied to the enemy controller yet. */
	void InitCoordinator(ASurvivalModeDirector* Director, USurvivalWaveSettings* WaveSettings);

	/** @brief Grace countdown started: configure the AI and start it when StartMoment is GracePeriodStart. */
	void OnGracePeriodStarted();

	/** @brief First wave phase: allow direct control, start thinking, follow the profile curves. */
	void OnWavesStarted();

	/** @brief Defeat: stop direct control and training when configured. */
	void OnWavesStopped();

	/** @brief Clears timers; called from the owning component's EndPlay. */
	void Shutdown();

	virtual UWorld* GetWorld() const override;

private:
	UPROPERTY()
	TWeakObjectPtr<ASurvivalModeDirector> M_Director;

	UPROPERTY()
	TWeakObjectPtr<USurvivalWaveSettings> M_WaveSettings;

	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController;

	bool bM_HasConfiguredAI = false;
	bool bM_ReportedMissingEnemyController = false;
	bool bM_WavesRunning = false;
	FTimerHandle M_ProfileUpdateTimerHandle;

	bool GetIsValidDirector() const;
	bool GetIsValidWaveSettings() const;

	/** @return The level's enemy controller, resolved lazily; reports once when the map has none. */
	AEnemyController* GetEnemyController();
	UEnemyStrategicAIComponent* GetStrategicAIComponent();
	bool GetShouldConfigure() const;

	// Runs once; after AEnemyController::BeginPlay copied its map settings so ours are not overwritten.
	void ConfigureStrategicAIOnce();
	void ApplyMissionSettings(const bool bAllowDirectControl);
	void ApplyDecisionTreeOverride();
	void StartStrategicThinking();
	void StartProfileUpdates();
	void UpdateFromProfile();

	const FSurvivalWaveDifficultyProfile* GetProfile() const;
	FRTSGameDifficulty GetDifficulty() const;
	float GetSurvivalMinute() const;
};
