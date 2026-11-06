// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "VehicleDuelBenchSubsystem.generated.h"

class ACPPController;
class ATankMaster;
class UUnitVariantDataAsset;
struct FMapTestArmorHitReport;
struct FMapTestShellLaunchReport;

enum class EVehicleDuelBenchStep : uint8
{
	StartGame,
	SpawnRun,
	Settle,
	Fight,
	Cleanup,
	Done
};

/** One tank of the running duel and everything it did and suffered. */
struct FVehicleDuelUnit
{
	TWeakObjectPtr<ATankMaster> Tank;
	// Hits and shells are matched by id: a killed tank or the shooter of a late shell may already be gone.
	uint32 TankUniqueId = 0;
	TWeakObjectPtr<ATankMaster> Target;
	FString Label;
	FString ClassName;
	// 0 = German side (player 1), 1 = Soviet side (player 2).
	int32 Side = 0;
	float MaxHealth = 0.f;
	float ExpectedRealisticMaxHealth = 0.f;
	// Set when the tank's max health differs from the Realistic data of the scenario subtype.
	FString HealthNote;
	bool bActivatesAmbush = false;
	bool bAmbushConfirmed = false;
	int32 ShellsFired = 0;
	int32 Hits = 0;
	int32 Bounces = 0;
	int32 Penetrations = 0;
	int32 OverPenetrations = 0;
	int32 ArmorDestroyedHits = 0;
	int32 FlankDetonations = 0;
	float DamageDealt = 0.f;
	int32 HitsTaken = 0;
	int32 NonFrontHitsTaken = 0;
	float ShellDamageTaken = 0.f;
	float EndHealth = 0.f;
	float FirstShotSeconds = -1.f;
	float DeathSeconds = -1.f;
	FString KilledBy;
	FString DeathCause;
};

/** Everything one scenario added up over its repeats, for the averages at the end of the summary. */
struct FVehicleDuelScenarioTotals
{
	int32 Runs = 0;
	int32 Timeouts = 0;
	int32 Wins[2] = {0, 0};
	// Indexed by the side that made the kill.
	int32 Kills[2] = {0, 0};
	float KillSecondsSum[2] = {0.f, 0.f};
	int32 HitsToKillSum[2] = {0, 0};
	int32 ArmorDestroyedHits[2] = {0, 0};
	int32 FlankDetonations[2] = {0, 0};
	int32 Bounces[2] = {0, 0};
	int32 Hits[2] = {0, 0};
	float FightSecondsSum = 0.f;
};

/**
 * @brief Realistic vehicle balance duel bench for tuning: launch with -game -VehicleDuelBench on a flat map, optionally
 * -VehicleDuelBenchScenario=<name> -VehicleDuelBenchRepeats=<n> -VehicleDuelBenchAt=X,Y -VehicleDuelBenchDir=<dir>.
 * Fights fixed frontal matchups, logs every shell (RTS_VDB lines), writes hits.csv and summary.md, then quits.
 * Map tests only: the body compiles to nothing without RTS_WITH_MAP_TESTS (every shipping build).
 */
UCLASS()
class RTS_SURVIVAL_API UVehicleDuelBenchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

#if RTS_WITH_MAP_TESTS

private:
	bool TryStartGame();
	void KeepGameRunning() const;
	bool ForceRealisticVehicleBalance() const;
	void BuildRunList();
	void OpenReportFiles();

	// ---- One run ----
	void SpawnRun();
	void SpawnSide(int32 ScenarioIndex, int32 Side, const FVector& ArenaCentre);
	bool SpawnDuelUnit(int32 ScenarioIndex, int32 SpecIndex, int32 Side, int32 CopyIndex, const FVector& Location,
	                   const FRotator& Facing);
	UClass* ResolveUnitClass(int32 ScenarioIndex, int32 SpecIndex) const;
	void TickSettle();
	void StartFight();
	void ActivateAmbush(FVehicleDuelUnit& Unit) const;
	void KeepGermanUnitsInEnemyVision() const;
	void TickFight();
	void RetargetUnits();
	void RecordDeaths();
	bool GetIsSideDefeated(int32 Side) const;
	void FinishRun(const TCHAR* Outcome);
	void TickCleanup();
	void DestroyRunActors();

	// ---- Hooks ----
	void OnShellLaunched(const FMapTestShellLaunchReport& Report);
	void OnArmorHit(const FMapTestArmorHitReport& Report);
	FVehicleDuelUnit* FindUnitByTank(const ATankMaster* Tank);
	FVehicleDuelUnit* FindUnitByUniqueId(uint32 UniqueId);
	void CheckRealisticMaxHealth(FVehicleDuelUnit& Unit) const;
	FVehicleDuelUnit* FindUnitByWeaponOwner(const UObject* WeaponOwner);
	void CountHitForShooter(FVehicleDuelUnit& Shooter, const FMapTestArmorHitReport& Report, float HealthLost);
	void AppendHitCsvLine(const FMapTestArmorHitReport& Report, const FString& ShooterLabel, const FString& VictimLabel,
	                      float HealthLost) const;

	// ---- Report ----
	void AppendRunToSummary(const TCHAR* Outcome, float FightSeconds);
	void AddRunToTotals(const TCHAR* Outcome, float FightSeconds);
	void AppendScenarioAverages();
	void WriteSummary() const;
	void AppendLine(const FString& FilePath, const FString& Line) const;

	bool TryGetGroundZ(const FVector& Location, float& OutGroundZ) const;
	float GetFightSeconds() const;
	float GetGameSeconds() const;
	void Finish(const TCHAR* Reason);

	TWeakObjectPtr<ACPPController> M_PlayerController;

	TWeakObjectPtr<const UUnitVariantDataAsset> M_VariantDataAsset;

	EVehicleDuelBenchStep M_Step = EVehicleDuelBenchStep::StartGame;
	// Scenario index per run, in the order they are fought.
	TArray<int32> M_RunScenarioIndices;
	TArray<int32> M_RunRepeatNumbers;
	int32 M_RunIndex = 0;
	TArray<FVehicleDuelUnit> M_Units;
	// Shooter unit index per shell launch id, so a shell landing after its tank died still has a shooter.
	TMap<uint32, int32> M_ShellShooterIndices;
	// Every actor spawned during a run; wrecks and units among them are removed before the next run.
	TArray<TWeakObjectPtr<AActor>> M_RunSpawnedActors;
	FDelegateHandle M_ActorSpawnedHandle;
	FDelegateHandle M_ShellLaunchedHandle;
	FDelegateHandle M_ArmorHitHandle;
	FVector M_ArenaCentre = FVector::ZeroVector;
	FString M_OutputDirectory;
	FString M_HitsCsvPath;
	FString M_SummaryPath;
	FString M_ScenarioFilter;
	TArray<FString> M_SummaryLines;
	TMap<int32, FVehicleDuelScenarioTotals> M_ScenarioTotals;
	int32 M_Repeats = 3;
	double M_StartRealSeconds = 0.0;
	float M_StepStartGameSeconds = 0.f;
	float M_FightStartGameSeconds = 0.f;
	float M_LastRetargetGameSeconds = 0.f;
	float M_SideDefeatedGameSeconds = -1.f;
	bool bM_IsActive = false;
	bool bM_RunFailed = false;
	// The first target pass orders every unit at its opposite number, even while its own target lives.
	bool bM_IsFirstTargetPass = false;

#endif
};
