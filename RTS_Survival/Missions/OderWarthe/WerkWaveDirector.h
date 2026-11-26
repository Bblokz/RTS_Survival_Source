// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"

#include "WerkWaveDirector.generated.h"

class ADestructableEnvActor;
class ADirectionFindingManager;
class AEnemyWaveSpawner;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWerkWaveEvent, int32, WaveNumber, int32, WerkIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWerkTunnelMouthCollapsed, int32, WerkIndex);

/** Wave timing for one difficulty (OderWartheMissionFlow.md section 5). */
USTRUCT(BlueprintType)
struct FWerkWaveTiming
{
	GENERATED_BODY()

	FWerkWaveTiming() = default;

	FWerkWaveTiming(const float InFirstWaveGraceSeconds, const float InIntervalSeconds,
	                const float InIntervalAfterNoticeSeconds)
		: FirstWaveGraceSeconds(InFirstWaveGraceSeconds)
		  , IntervalSeconds(InIntervalSeconds)
		  , IntervalAfterNoticeSeconds(InIntervalAfterNoticeSeconds)
	{
	}

	// Seconds after the mission asks for it (M04) before the first wave goes, unless DF 1 is taken sooner.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0.0"))
	float FirstWaveGraceSeconds = 300.f;

	// Seconds between timed waves before the command post notices the player (two stations held).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "10.0"))
	float IntervalSeconds = 180.f;

	// Seconds between timed waves once it noticed them.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "10.0"))
	float IntervalAfterNoticeSeconds = 150.f;
};

/** Book-keeping for one launched wave. */
USTRUCT()
struct FWerkWaveRecord
{
	GENERATED_BODY()

	int32 WaveNumber = 0;
	int32 WerkIndex = INDEX_NONE;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Units;

	bool bHasSpawned = false;
	bool bIsDefeated = false;
};

/**
 * @brief Place one in Ger_OderWarthe: sends the Werk attack waves. Each wave comes from the tunnel mouth of a random
 * bunker group that still stands (not eliminated in the hunt, its tunnel mouth not collapsed); the spawner's own
 * units and route make the wave. When the wave spawns, every held listening station hears the attack order, and
 * from the second wave on the Advisor warns (LaterWaveWarnings; M04 warns for the first).
 * Missions call LaunchWave (M04's first wave) and StartTimedWaves (from M05), and listen to OnWaveLaunched and
 * OnWaveDefeated. Placed by ow_map_build.step_waves(); rules in OderWartheMissionFlow.md sections 4.4 and 5.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AWerkWaveDirector : public AEnemyPlacementActor
{
	GENERATED_BODY()

public:
	AWerkWaveDirector();

	virtual FColor GetPlacementColor() const override;
	virtual FString GetConfigurationProblem() const override;

	/**
	 * @brief Sends one wave now from a random standing Werk; it counts as launched once its units spawn.
	 * @return The wave's number (1 = the first), or 0 when no Werk can send one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Werk Waves")
	int32 LaunchWave();

	/** Sends a wave every interval of the difficulty (shorter once the command post noticed the player). */
	UFUNCTION(BlueprintCallable, Category = "Werk Waves")
	void StartTimedWaves();

	UFUNCTION(BlueprintCallable, Category = "Werk Waves")
	void StopTimedWaves();

	/** @return The first wave's grace time on the current difficulty (seconds). */
	UFUNCTION(BlueprintPure, Category = "Werk Waves")
	float GetFirstWaveGraceSeconds() const;

	/** @return Waves asked for so far (also the ones whose units have not spawned yet). */
	UFUNCTION(BlueprintPure, Category = "Werk Waves")
	int32 GetWavesLaunched() const { return M_Waves.Num(); }

	/** @return The map's wave director, or null (no error) on maps without one. */
	UFUNCTION(BlueprintPure, Category = "Werk Waves", meta = (WorldContext = "WorldContextObject"))
	static AWerkWaveDirector* FindWerkWaveDirector(const UObject* WorldContextObject);

	// A wave's units spawned and set off; the held stations heard the attack order.
	UPROPERTY(BlueprintAssignable, Category = "Werk Waves")
	FOnWerkWaveEvent OnWaveLaunched;

	// Every unit of the wave is dead.
	UPROPERTY(BlueprintAssignable, Category = "Werk Waves")
	FOnWerkWaveEvent OnWaveDefeated;

	// Heavy guns brought a Werk's tunnel mouth down: that Werk sends no more waves.
	UPROPERTY(BlueprintAssignable, Category = "Werk Waves")
	FOnWerkTunnelMouthCollapsed OnTunnelMouthCollapsed;

	UFUNCTION(BlueprintPure, Category = "Werk Waves")
	int32 GetCollapsedTunnelMouthCount() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FString GetPlacementKindName() const override;
	virtual FName GetPlacementId() const override { return TEXT("WerkWaves"); }
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const override;
	virtual bool GetNeedsEnemyController() const override { return false; }

	// One attack-wave spawner per Werk, in the direction-finding manager's Werk order. Set by the map build.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Werk Waves|Map")
	TArray<TObjectPtr<AEnemyWaveSpawner>> WerkSpawners;

	// The Werke's BP_OW_TunnelMouth, in the same order as WerkSpawners. Set by the map build.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Werk Waves|Map")
	TArray<TObjectPtr<ADestructableEnvActor>> TunnelMouths;

	// Played (one at a time, alternating) when the second and every later wave spawns.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Werk Waves|Lines")
	TArray<TSoftObjectPtr<USoundBase>> LaterWaveWarnings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Werk Waves|Lines")
	ERTSPortraitTypes WarningPortrait = ERTSPortraitTypes::GermanAnnouncer;

	// Set by the map build; without it every spawner counts as standing and no bursts are sent.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Werk Waves|Map")
	TObjectPtr<ADirectionFindingManager> DirectionFinding;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Werk Waves|Timing")
	FWerkWaveTiming TimingNormal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Werk Waves|Timing")
	FWerkWaveTiming TimingHard = FWerkWaveTiming(240.f, 150.f, 120.f);

	// Also Ironman.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Werk Waves|Timing")
	FWerkWaveTiming TimingBrutal = FWerkWaveTiming(180.f, 120.f, 105.f);

	// Seed of the Werk pick; 0 seeds from the clock.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Werk Waves")
	int32 RandomSeed = 0;

private:
	UPROPERTY()
	TArray<FWerkWaveRecord> M_Waves;

	FRandomStream M_Random;
	// Per Werk (index into TunnelMouths): its tunnel mouth collapsed.
	TArray<bool> M_TunnelMouthCollapsed;
	int32 M_LaterWarningsPlayed = 0;
	FTimerHandle M_TimedWaveHandle;
	FTimerHandle M_DefeatCheckHandle;
	bool bM_TimedWavesRunning = false;

	const FWerkWaveTiming& GetTiming() const;
	bool GetIsWerkStanding(const int32 WerkIndex) const;
	TArray<int32> GetStandingWerke() const;
	bool GetIsValidDirectionFinding() const;

	void BindSpawners();
	void UnbindSpawners();
	void BindTunnelMouths();
	void HandleTunnelMouthDied(const int32 WerkIndex);
	void PlayLaterWaveWarning(const int32 WaveNumber);

	UFUNCTION()
	void HandleSpawnerUnitsSpawned(AEnemyWaveSpawner* Spawner, const TArray<AActor*>& Units);

	// The wave of this Werk that is waiting for its units; INDEX_NONE (into M_Waves) when none waits.
	int32 FindWaitingWave(const int32 WerkIndex) const;

	void ScheduleNextTimedWave();
	void HandleTimedWave();
	void EnsureDefeatCheck();
	void CheckDefeatedWaves();
	static bool GetIsWaveAlive(const FWerkWaveRecord& Wave);
};
