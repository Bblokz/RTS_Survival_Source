// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/EnemyAirWarnings/EnemyAirWarningSettings.h"

#include "EnemyAirWarningTestSubsystem.generated.h"

/**
 * @brief -EnemyAirWarningsValidate on a map with an enemy controller (UT_EnemyAI): checks every warning has its
 * abilities and loadable portrait lines, the ability -> warning mapping, that an air ability fired by the enemy
 * abilities manager queues a warning line (when the map's enemy has one loaded), and the same-warning / between-
 * warnings cooldowns. Writes Saved/EnemyAirWarningTest/results.txt and quits. Only built with RTS_WITH_MAP_TESTS.
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyAirWarningTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(const float DeltaTime) override;
	// The controller pauses mission maps at start; the test unpauses it.
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	enum class EPhase : uint8
	{
		WaitingForStart,
		WaitingForGap,
		WaitingForPortrait,
		Finished
	};

	EPhase M_Phase = EPhase::WaitingForStart;
	double M_PhaseStartSeconds = -1.0;
	// The warning the first step played; the gap step plays a different one.
	EEnemyAirWarning M_FirstWarning = EEnemyAirWarning::None;

	TArray<FString> M_Lines;
	int32 M_Failed = 0;

	void SetPhase(const EPhase NewPhase);
	double GetPhaseSeconds() const;
	void StartTest();
	void CheckSettings();
	void CheckAbilityMapping();
	void CheckFirstWarning();
	bool TryFireLoadedEnemyAirAbility(EEnemyAirWarning& OutWarning);
	void CheckCooldowns();
	void CheckWarningAfterGap();
	bool GetIsLastPlayedLineOf(EEnemyAirWarning Warning) const;
	void Evaluate();
	void Check(const bool bCondition, const FString& Name, const FString& Detail);
};
