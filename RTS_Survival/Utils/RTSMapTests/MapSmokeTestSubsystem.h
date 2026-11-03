// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "MapSmokeTestSubsystem.generated.h"

class ACPPController;
enum class EMapSmokeVictoryStep : uint8;
class FMapSmokeTestLogCounter;

/**
 * @brief Plays a real map unattended to smoke-test a packaged build: with -RTSMapSmokeTest it starts the game the way
 * the player's "Press to start" does (choosing Normal on a difficulty picker), lets it run for
 * -RTSMapSmokeTestSeconds=N game seconds (default 180), logs the error and ensure counts per log category and quits.
 * With -RTSMenuFlowTest it first follows the player's route from the main menu map: the Load Campaign button, then
 * the first mission on the campaign map, and smoke-tests that mission.
 * With -RTSMapSmokeTestVictory the mission run ends in a victory whose Continue button must bring the game back to
 * the campaign map.
 * With -RTSMapSmokeTestPIESettings the game instance gets the PIE startup settings (faction, commander, difficulty,
 * campaign) first, as a play-in-editor session does; a packaged build otherwise runs on the code fallbacks.
 * Not created in a shipping build.
 */
UCLASS()
class RTS_SURVIVAL_API UMapSmokeTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	// Counts error lines and ensures from every thread while the map runs; registered with GLog.
	TSharedPtr<FMapSmokeTestLogCounter> M_LogCounter;

	float M_RunSeconds = 0.f;
	float M_GameSecondsAtStart = 0.f;
	float M_RealSecondsAtStart = 0.f;
	float M_NextHeartbeatSeconds = 0.f;
	bool bM_HasStartedGame = false;
	bool bM_HasFinished = false;
	bool bM_HasPressedLoadCampaign = false;
	bool bM_HasPressedSurvival = false;
	// Steps of the survival route through the faction map: 0 faction, 1 variants, 2 difficulty, 3 done.
	int32 M_SurvivalFactionStep = 0;
	float M_NextSurvivalStepSeconds = 0.f;
	// Real time the game has been paused since (-1 while running); a locked pause the test cannot lift ends the run.
	float M_PausedSinceRealSeconds = -1.f;
	float M_NextMissionLaunchSeconds = 0.f;
	// Victory round trip: which step runs and when it started.
	uint8 M_VictoryStep = 0;
	float M_VictoryStepStartSeconds = 0.f;

	// Menu flow: returns true while this world is the main menu or the campaign map, which the flow drives itself.
	bool TickMenuFlow();
	void PressLoadCampaignButton();
	bool TickSurvivalMenuFlow();
	void PressSurvivalButton();
	void TickSurvivalFactionFlow(class AFactionPlayerController* FactionController);
	void LaunchFirstCampaignMission();
	void ApplyPIEStartupSettingsOnce() const;
	bool TryStartGame();
	void DismissDifficultyPicker() const;
	// Tutorial popups pause and lock the game until the player presses Continue; the smoke test presses it.
	void ContinueTutorialOvertakes() const;
	void KeepGameUnpaused() const;
	// Ends the run with a FAIL naming the on-screen widgets when the game stays paused (defeat, a locked dialog).
	bool TickStuckPause(const float RealSecondsRun);
	void LogHeartbeat(const float SecondsRun, const float RealSecondsRun);
	void Finish();
	// Victory round trip: triggers the mission victory, presses Continue and waits for the travel.
	void TickVictoryRoundTrip(const float WorldSeconds);
	// On the campaign map after a victory: reports the round trip once the campaign settled.
	bool TickReturnedFromVictory();
	void LogResult(const FString& Scenario, const bool bPassed, const FString& Details);
	// Living tanks, nomadic vehicles, aircraft and building expansions the game unit manager does not report under
	// their own subtype; missions and global abilities read those counters.
	int32 CountUnitSubtypeCounterMismatches() const;
	ACPPController* GetController() const;
};
