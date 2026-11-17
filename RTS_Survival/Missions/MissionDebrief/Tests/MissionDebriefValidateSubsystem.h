// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Missions/MissionDebrief/MissionDebriefTypes.h"

#include "MissionDebriefValidateSubsystem.generated.h"

class AMissionManager;

enum class EMissionDebriefValidatePhase : uint8
{
	WaitingForHud,
	StartingGame,
	Watching,
	Done
};

/**
 * @brief Headless check of the mission end screen, started with -MissionDebriefValidate (add -MissionDebriefDefeat for
 * the defeat run; best with -game -RenderOffscreen on a unit test map): ends the mission through the map's mission
 * manager, screenshots the debrief at fixed real times to Saved/MissionDebrief/<outcome>/, and checks that the stage
 * spawned and took the view, that the game stayed paused, and that the buttons rose in on time. Grep MISSION_DEBRIEF.
 */
UCLASS()
class RTS_SURVIVAL_API UMissionDebriefValidateSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	EMissionDebriefValidatePhase M_Phase = EMissionDebriefValidatePhase::WaitingForHud;
	EMissionDebriefOutcome M_Outcome = EMissionDebriefOutcome::Victory;
	double M_PhaseStartSeconds = 0.0;
	int32 M_NextShotIndex = 0;
	int32 M_Failures = 0;
	bool bM_CheckedStage = false;
	bool bM_CheckedButtons = false;

	TWeakObjectPtr<AMissionManager> M_MissionManager;

	void EnterPhase(const EMissionDebriefValidatePhase NewPhase);
	double GetSecondsInPhase() const;
	void TickWaitingForHud();
	void TickStartingGame();
	void TickWatching();
	void CheckStage();
	void CheckButtons();
	void TriggerMissionEnd();
	void DismissDifficultyPicker() const;
	void Capture(const FString& Label) const;
	void Fail(const FString& Reason);
	void Finish();
};
