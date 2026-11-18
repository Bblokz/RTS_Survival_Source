// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "FourPlayerSkirmishSubsystem.generated.h"

class ACPPController;
class AEnemyController;
struct FStrategicAIResultBatch;
class UEnemyControllerSubsystem;
struct FMapTestDamageReport;

enum class EFourPlayerSkirmishStep : uint8
{
	StartGame,
	Spawn,
	Fight,
	Done
};

enum class EFourPlayerSkirmishPhase : uint8
{
	// Every player on its own team.
	FreeForAll,
	// Players 1 and 3 against players 2 and 4.
	Teams
};

enum class EFourPlayerSkirmishUnitKind : uint8
{
	Tank,
	Squad,
	Turret
};

/** One spawned tank, squad or standalone turret of the skirmish and everything it did and suffered. */
struct FFourPlayerSkirmishUnit
{
	TWeakObjectPtr<AActor> Actor;
	FString Label;
	FString ClassName;
	int32 OwningPlayer = RTSPlayers::NeutralOwner;
	EFourPlayerSkirmishUnitKind Kind = EFourPlayerSkirmishUnitKind::Tank;
	// Soldiers of a squad, known once the squad has loaded them; empty for tanks and turrets.
	TArray<TWeakObjectPtr<AActor>> Soldiers;
	// Everything of the unit that holds weapon states: turrets, hull weapons, infantry weapons, the turret itself.
	TArray<TWeakObjectPtr<UObject>> WeaponCarriers;
	int32 WeaponCount = 0;
	// Longest UWeaponState::GetRange of the unit at the setup gate; used to check the layout against real ranges.
	float MaxWeaponRangeCm = 0.f;
	bool bIsSetupChecked = false;
	bool bIsSetupOk = false;
	// Indexed by attacking player; slot 0 is damage no damage source claimed.
	float DamageTakenFromPlayer[RTSPlayers::PlayerArraySize] = {};
	int32 HitsTakenFromPlayer[RTSPlayers::PlayerArraySize] = {};
	// Indexed by the victim's owner. Inferred: damage of this unit's player counts for the unit when it held the
	// victim as target shortly before; the damage hook names the attacking player, not the attacking unit.
	float DamageDealtToPlayer[RTSPlayers::PlayerArraySize] = {};
	int32 HitsOnPlayer[RTSPlayers::PlayerArraySize] = {};
	// Indexed by the victim's owner. Exact: hits whose damage hook named one of this unit's weapons as the shooter.
	float ExactDamageDealtToPlayer[RTSPlayers::PlayerArraySize] = {};
	int32 ExactHitsOnPlayer[RTSPlayers::PlayerArraySize] = {};
	// Indexed by the owner of the target the firing weapon held; slot 0 = fired without a held target owned by a player.
	int32 FireEventsAtPlayer[RTSPlayers::PlayerArraySize] = {};
	// Seconds a weapon of the unit held a target owned by that player, from sampling the held targets.
	float TargetSecondsByPlayer[RTSPlayers::PlayerArraySize] = {};
	// Fight seconds at which this unit last held each other unit as target, by unit index.
	TArray<float> LastTargetedUnitSeconds;
	// Samples in which the unit held a target of its own player or an ally: never expected.
	int32 NonHostileTargetSamples = 0;
	float DeathSeconds = -1.f;
};

/** A tank spawned out of the fight whose owner the harness changes at runtime between the human and an AI player. */
struct FFourPlayerSkirmishOwnerChangeCase
{
	TWeakObjectPtr<AActor> Actor;
	FString Label;
	int32 FromPlayer = RTSPlayers::NeutralOwner;
	int32 ToPlayer = RTSPlayers::NeutralOwner;
};

/**
 * One result of a player 3 or 4 controller's strategic AI "count the enemy's units" query, next to the squad counts
 * the unit manager has for three readings of "enemy": hostile players, the human only (the old two-player rule) and
 * every other player (allies included).
 */
struct FFourPlayerSkirmishStrategicSample
{
	int32 Player = RTSPlayers::NeutralOwner;
	float FightSeconds = 0.f;
	int32 ReportedSquads = 0;
	int32 HostileSquads = 0;
	int32 HumanSquads = 0;
	int32 OtherPlayersSquads = 0;
	bool bIsConclusive = false;
};

/** One unit an enemy controller's attack wave spawned, and whether its weapons are counted yet. */
struct FFourPlayerSkirmishWaveUnit
{
	TWeakObjectPtr<AActor> Actor;
	FString Name;
	int32 ReportedOwner = INDEX_NONE;
	float ArrivedGameSeconds = -1.f;
	int32 WeaponCount = 0;
	int32 WeaponOwnerMismatches = 0;
	bool bAreWeaponsBound = false;
	FVector StartLocation = FVector::ZeroVector;
	// Furthest the unit got from where it started; a strategy order must move it into the fight.
	float MaxMovedCm = 0.f;
};

/**
 * An attack wave that the enemy controller of player 3 or 4 starts itself (StartWaveFromRequest: its own spawning,
 * formation and attack-move orders) from behind its group into the arena, and what the wave's units then did.
 */
struct FFourPlayerSkirmishControllerWave
{
	// False: an attack wave the controller starts. True: the controller's decision tree orders its idle probe tank.
	bool bIsStrategyOrder = false;
	int32 Player = RTSPlayers::NeutralOwner;
	TWeakObjectPtr<AEnemyController> Controller;
	int32 WaveId = INDEX_NONE;
	float StartGameSeconds = -1.f;
	TArray<FFourPlayerSkirmishWaveUnit> Units;
	// Indexed by the owner of the target the firing weapon held; slot 0 = no player's target.
	int32 FireEventsAtPlayer[RTSPlayers::PlayerArraySize] = {};
	// Indexed by the victim's owner; hits whose damage hook named a weapon of a unit of this wave.
	float ExactDamageToPlayer[RTSPlayers::PlayerArraySize] = {};
	int32 ExactHitsOnPlayer[RTSPlayers::PlayerArraySize] = {};
	FString Result;
};

/**
 * One later AI player's (3, 4) enemy controller and the probe unit spawned through the controller's async spawn
 * path. The probe spawns far behind its own group and stays out of the unit records; once its owner is checked it
 * is handed to the controller, and only the controller's decision tree can order it into the fight.
 */
struct FFourPlayerSkirmishControllerProbe
{
	int32 Player = RTSPlayers::NeutralOwner;
	TWeakObjectPtr<AEnemyController> Controller;
	FString ControllerName;
	// True when the map had no controller for the player and the harness spawned one.
	bool bIsSpawnedByHarness = false;
	// Spawner function the probe went through; empty until requested.
	FString SpawnPath;
	int32 SpawnOwnerOverride = INDEX_NONE;
	TWeakObjectPtr<AActor> SpawnedUnit;
	FString SpawnedUnitName;
	float RequestGameSeconds = -1.f;
	bool bIsRequested = false;
	// The spawner called back; bWasUnitDelivered tells a delivered unit from a reported spawn failure.
	bool bHasCallbackRun = false;
	bool bWasUnitDelivered = false;
	bool bIsResolved = false;
	int32 ReportedOwner = INDEX_NONE;
	int32 WeaponCount = 0;
	int32 WeaponMismatches = 0;
	FString Result;
};

/**
 * @brief Four-player playtest: launch with -game -FourPlayerSkirmish on a flat map, optionally
 * -FourPlayerSkirmishPhase=FreeForAll|Teams -FourPlayerSkirmishDir=<dir> -FourPlayerSkirmishAt=X,Y
 * -FourPlayerSkirmishSide=<cm> -FourPlayerSkirmishSeconds=<n>. Spawns a tank, a squad and a turret for each of the
 * four players, counts who really damaged whom, writes hits.csv and summary.md (RTS_FPS log lines), then quits.
 * Players 3 and 4 also get an enemy controller each (spawned when the map has none); a probe unit spawned through
 * each controller's async spawn path must come up owned by that controller's player.
 * Map tests only: the body compiles to nothing without RTS_WITH_MAP_TESTS (every shipping build).
 */
UCLASS()
class RTS_SURVIVAL_API UFourPlayerSkirmishSubsystem : public UTickableWorldSubsystem
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
	void ReadCommandLine();
	void TickStartGame(double RealSeconds);
	bool TryStartGame();
	void KeepGameRunning() const;
	void OpenReportFiles();

	// ---- Spawning ----
	void SpawnPhase();
	void ApplyPhaseTeams() const;
	/** @return Whether the relations really are the ones the phase asked for (teams change from the next shot on). */
	bool GetAreTeamsApplied() const;
	void SpawnGroup(int32 OwningPlayer, const FVector& ArenaCentre);
	/**
	 * @brief Spawns one unit of a group for its player and starts its record.
	 * @param OwningPlayer Player the unit is spawned for.
	 * @param UnitSlot Index of the unit in the group table (tank, squad, turret).
	 * @param GroundLocation Where the unit stands, on the ground.
	 * @param Facing Rotation toward the arena centre.
	 * @return False when the class is missing or the spawn failed.
	 */
	bool SpawnSkirmishUnit(int32 OwningPlayer, int32 UnitSlot, const FVector& GroundLocation, const FRotator& Facing);

	// ---- Enemy controllers of players 3 and 4 ----
	void SetUpEnemyControllers();
	/** @return The map's controller for the player, else one spawned now; null when spawning it failed. */
	AEnemyController* FindOrSpawnEnemyController(const UEnemyControllerSubsystem& ControllerSubsystem, int32 Player,
	                                             bool& bOutIsSpawnedByHarness);
	AEnemyController* SpawnEnemyController(int32 Player);
	/**
	 * @brief Checks the registry after players 3 and 4 got their controllers: each is found under its own player and
	 * the argument-less getter still serves player 2's.
	 * @param ControllerSubsystem The world's controller registry.
	 * @param MapPlayer2Controller Player 2's controller read before the harness spawned any; null when the map has none.
	 */
	void CheckEnemyControllerRegistry(const UEnemyControllerSubsystem& ControllerSubsystem,
	                                  const AEnemyController* MapPlayer2Controller);
	void CheckProbeRegistration(const UEnemyControllerSubsystem& ControllerSubsystem,
	                            const FFourPlayerSkirmishControllerProbe& Probe);
	void RequestControllerProbeSpawns();
	void RequestControllerProbeSpawn(int32 ProbeIndex);
	void OnControllerProbeSpawned(int32 ProbeIndex, AActor* SpawnedActor);
	void TickControllerProbes();
	/**
	 * @brief Builds a throwaway unit record of a probe so the weapon carrier and owner checks of the groups apply.
	 * @param Probe Probe whose spawned unit is checked.
	 * @param OutWeaponMismatches Weapon states and turrets that name another player than the controller's.
	 * @return The record; its WeaponCount is the number of weapon states found.
	 */
	FFourPlayerSkirmishUnit MakeProbeUnitRecord(const FFourPlayerSkirmishControllerProbe& Probe,
	                                            int32& OutWeaponMismatches) const;
	void ResolveControllerProbe(FFourPlayerSkirmishControllerProbe& Probe);
	// Timeout when the spawner never called back; a failed spawn when it called back without a (living) unit.
	void ResolveMissingControllerProbe(FFourPlayerSkirmishControllerProbe& Probe);
	void ResolvePendingControllerProbes();
	bool GetAreControllerProbesResolved() const;
	FVector GetControllerProbeLocation(int32 Player) const;
	void AddControllerFailure(const FString& Reason);

	// ---- Runtime owner change between the human and an AI player ----
	void SpawnOwnerChangeUnits();
	/** At the setup gate: weapon traces of all four players before and after each tank changes owner. */
	void RunOwnerChangeChecks();
	void CheckOwnerChangeCase(const FFourPlayerSkirmishOwnerChangeCase& Case);
	/**
	 * @brief Checks that exactly the players hostile to the expected owner hit the tank with a real weapon query.
	 * @param Case Tank under test.
	 * @param ExpectedOwner Owner the tank should have now.
	 * @param StageName "before" or "after", for the report.
	 * @return Number of players whose shot did the wrong thing.
	 */
	int32 CheckShotsAtOwnerChangeUnit(const FFourPlayerSkirmishOwnerChangeCase& Case, int32 ExpectedOwner,
	                                  const TCHAR* StageName);
	/** @return Whether a weapon trace of the player, with its channel and ignore mask, stops on the target. */
	bool GetDoesShotOfPlayerHitActor(int32 Player, const AActor& Target) const;
	void AppendOwnerChangeToSummary(TArray<FString>& OutLines) const;

	// ---- Strategic AI of the player 3 and 4 controllers ----
	/** Starts the controllers' own strategic AI thinking and listens to the results it applies. */
	void StartControllerStrategicAI();
	/** Queues a unit count query on each controller's strategic AI; its think loop sends it with its own requester. */
	void QueueControllerUnitCountQueries();
	void OnControllerStrategicResults(int32 Player, const FStrategicAIResultBatch& ResultBatch);
	bool GetHasConclusiveStrategicSample(int32 Player) const;
	bool GetAreControllerStrategicChecksSettled() const;
	void AppendStrategicAIReasons(TArray<FString>& OutReasons) const;
	void AppendStrategicAIToSummary(TArray<FString>& OutLines) const;

	// ---- Attack waves of the player 3 and 4 controllers ----
	/** Each later AI player's controller starts one attack-move wave of its own from behind its group to the arena. */
	void StartControllerWaves();
	void StartControllerWave(int32 WaveIndex);
	void OnControllerWaveUnitsSpawned(int32 WaveIndex, const TArray<AActor*>& SpawnedUnits);
	/** Hands a resolved probe tank to its controller's strategic AI as an idle unit for the decision tree to order. */
	void StartControllerStrategyOrder(const FFourPlayerSkirmishControllerProbe& Probe);
	bool GetAreControllerStrategyOrdersSettled() const;
	/** Binds the fire counters of wave units once their turrets have set up their weapon states. */
	void TickControllerWaves();
	void BindWaveUnitWeapons(int32 WaveIndex, FFourPlayerSkirmishWaveUnit& WaveUnit);
	void OnWaveWeaponFired(int32 WaveIndex, const TWeakObjectPtr<UObject>& Carrier);
	/** @return Index of the controller wave that owns the actor (or the unit it is attached to), or INDEX_NONE. */
	int32 FindControllerWaveIndexOfActor(const AActor* Actor) const;
	/** @return True when the damage hook named a weapon of a controller wave unit and the hit was credited to it. */
	bool CreditControllerWaveHit(const FMapTestDamageReport& Report, int32 VictimOwner);
	/** @return Whether a wave has engaged (fired at and hit a hostile player) or can no longer do so. */
	bool GetIsControllerWaveSettled(const FFourPlayerSkirmishControllerWave& Wave) const;
	bool GetAreControllerWavesSettled() const;
	void AppendControllerWaveReasons(TArray<FString>& OutReasons) const;
	void AppendControllerWavesToSummary(TArray<FString>& OutLines) const;

	// ---- Setup gate ----
	void TickSetupGate();
	bool GetAreAllSquadsLoaded() const;
	bool GetDoAllUnitsHaveWeapons() const;
	void RegisterSquadSoldiers(int32 UnitIndex);
	void CollectWeaponCarriers(FFourPlayerSkirmishUnit& Unit) const;
	void RunSetupGate();
	void CheckUnitSetup(FFourPlayerSkirmishUnit& Unit);
	int32 CountSoldierOwnerMismatches(const FFourPlayerSkirmishUnit& Unit) const;
	/** @return Weapon states and turret owners that name another player; also refreshes Unit.WeaponCount. */
	int32 CountWeaponOwnerMismatches(FFourPlayerSkirmishUnit& Unit) const;
	float GetMaxWeaponRange(const FFourPlayerSkirmishUnit& Unit) const;
	// Logs, per player, how many other groups its longest weapon reaches with the real weapon ranges.
	void CheckGroupReach();
	/**
	 * @brief Measures one player's group against every other group, for the reach list in the summary.
	 * @param Player Player whose reach is measured.
	 * @param LongestRangeCm Longest weapon range of the player's units.
	 * @param OutDistances One "P<n> <m> m" entry per other player.
	 * @return Number of hostile groups whose closest member is within LongestRangeCm.
	 */
	int32 CountHostileGroupsInReach(int32 Player, float LongestRangeCm, TArray<FString>& OutDistances) const;
	float GetClosestDistanceBetweenPlayers(int32 PlayerA, int32 PlayerB) const;
	// Warns about units whose weapons never held a target: a turret that never engaged passes the per-player verdict.
	void WarnAboutUnitsThatNeverTargeted();

	// ---- Fight ----
	void TickFight();
	void KeepHumanUnitsInEnemyVision() const;
	void SampleHeldTargets();
	/** Counts every round or burst each weapon of the spawned units fires, by the owner of the target it held. */
	void BindWeaponFireCounters();
	void OnWeaponFired(int32 UnitIndex, const TWeakObjectPtr<UObject>& Carrier);
	/**
	 * @brief Credits a hit to the unit whose weapon the damage hook named, when it names one of the spawned units.
	 * @return The unit's label, or empty when the hook named no spawned unit.
	 */
	FString CreditExactAttackingUnit(const FMapTestDamageReport& Report, int32 AttackerSlot, int32 VictimOwner);
	void SampleUnitTargets(int32 UnitIndex, float SampleSeconds);
	/**
	 * @brief Notes one held target of a unit: who owns it, and that the unit aimed at it now (for crediting hits).
	 * @param UnitIndex Unit whose weapon holds the target.
	 * @param Target Held target actor.
	 * @param InOutTargetedOwnersMask Player bits of the owners targeted in this sample, so seconds count once.
	 */
	void RecordHeldTarget(int32 UnitIndex, const AActor* Target, uint8& InOutTargetedOwnersMask);
	void RecordDeaths();
	bool GetIsUnitAlive(const FFourPlayerSkirmishUnit& Unit) const;
	int32 GetLivingPlayerCount() const;
	bool GetShouldEndFight();
	void LogStatus();

	// ---- Damage ----
	void OnDamageApplied(const FMapTestDamageReport& Report);
	void RecordDamageOnOtherVictim(const FMapTestDamageReport& Report, int32 AttackerSlot);
	int32 FindUnitIndexByActor(const AActor* Actor) const;
	int32 FindUnitIndexOfSoldierSquad(const AActor* Actor) const;
	/**
	 * @brief Gives a hit to the units of the attacking player that held the victim as target shortly before.
	 * @param AttackingPlayer Player the damage hook named.
	 * @param VictimUnitIndex Unit that was hit, or INDEX_NONE when the victim is not one of the spawned units.
	 * @param VictimOwner Owner of the victim.
	 * @param AppliedDamage Health really lost.
	 * @return Labels of the credited units joined with '+', empty when no unit held the victim.
	 */
	FString CreditAttackingUnits(int32 AttackingPlayer, int32 VictimUnitIndex, int32 VictimOwner, float AppliedDamage);
	void AppendHitCsvLine(const FMapTestDamageReport& Report, const FString& AttackerUnits, const FString& VictimLabel,
	                      int32 VictimOwner) const;

	// ---- Verdict and report ----
	/** @return True when the phase criteria hold; otherwise OutReasons names every criterion that does not. */
	bool EvaluateCriteria(TArray<FString>& OutReasons) const;
	void AppendIntegrityReasons(TArray<FString>& OutReasons) const;
	/** @return Whether every player has damaged as many hostile players as the phase asks; ends the fight early. */
	bool GetIsDamageRequirementMet() const;
	int32 GetRequiredHostilePlayersDamaged() const;
	int32 CountHostilePlayersDamagedBy(int32 AttackingPlayer) const;
	float GetUnclaimedDamageOnUnits() const;
	void RecheckWeaponOwnersAtEnd();
	void FinishPhase();
	void BuildSummary(bool bPassed, const TArray<FString>& Reasons, TArray<FString>& OutLines) const;
	void AppendMatrixToSummary(TArray<FString>& OutLines) const;
	void AppendPlayersToSummary(TArray<FString>& OutLines) const;
	void AppendUnitsToSummary(TArray<FString>& OutLines) const;
	/** Per weapon kind (tank turrets, infantry weapons, standalone turrets): fire events and exact hits by player. */
	void AppendWeaponKindsToSummary(TArray<FString>& OutLines) const;
	void AppendControllersToSummary(TArray<FString>& OutLines) const;
	void LogMatrix() const;
	FString GetPhaseName() const;

	void AppendLine(const FString& FilePath, const FString& Line) const;
	bool TryGetGroundZ(const FVector& Location, float& OutGroundZ) const;
	float GetFightSeconds() const;
	float GetGameSeconds() const;
	/**
	 * @brief Writes summary.md and ends the process with the RESULT line the runner looks for. Every exit, early
	 * failures included, goes through here so a RESULT line always exists.
	 * @param bPassed Verdict of the phase.
	 * @param Reasons Why it failed; empty for a pass.
	 */
	void Finish(bool bPassed, const TArray<FString>& Reasons);

	TWeakObjectPtr<ACPPController> M_PlayerController;

	EFourPlayerSkirmishStep M_Step = EFourPlayerSkirmishStep::StartGame;
	EFourPlayerSkirmishPhase M_Phase = EFourPlayerSkirmishPhase::FreeForAll;
	TArray<FFourPlayerSkirmishUnit> M_Units;
	// Unit index per unit actor and per soldier, so a hit soldier or a held target resolves to its unit.
	TMap<TWeakObjectPtr<const AActor>, int32> M_UnitIndexByActor;
	// [attacking player][victim's owner]; attacker slot 0 is damage no damage source claimed.
	float M_DamageMatrix[RTSPlayers::PlayerArraySize][RTSPlayers::PlayerArraySize] = {};
	int32 M_HitMatrix[RTSPlayers::PlayerArraySize][RTSPlayers::PlayerArraySize] = {};
	// Damage of a player that no unit of it could be credited with: no unit held the victim as target.
	float M_UnitUncreditedDamage[RTSPlayers::PlayerArraySize] = {};
	// Health lost by things that belong to none of the spawned groups (wrecks, scenery, map-placed units).
	float M_OtherVictimDamage = 0.f;
	int32 M_OtherVictimHits = 0;
	// Damage of a player on a non-spawned thing owned by itself or an ally: friendly fire all the same.
	float M_OtherVictimNonHostileDamage = 0.f;
	// Hits on a spawned unit whose owner, as the game reports it, is not the player it was spawned for.
	int32 M_VictimOwnerDriftHits = 0;
	// Damage claimed by an owner outside 1..MaxPlayers (neutral, unset): attribution names no real player.
	int32 M_OddAttackerHits = 0;
	// Hits the damage hook credited to one of the spawned units, and those whose shooter's player differs from the
	// player the hook named (a weapon state firing for the wrong player).
	int32 M_ExactlyCreditedHits = 0;
	int32 M_ShooterOwnerMismatchHits = 0;
	int32 M_DamageReportCount = 0;
	int32 M_SpawnFailureCount = 0;
	int32 M_SetupMismatchCount = 0;
	// Weapon owner mismatches on the living units at the end; weapons can be created after the setup gate.
	int32 M_EndWeaponMismatchCount = 0;
	TArray<FString> M_SetupLines;
	TArray<FString> M_Warnings;
	// Filled once in the spawn step and never resized after: spawn callbacks address probes by index.
	TArray<FFourPlayerSkirmishControllerProbe> M_ControllerProbes;
	TArray<FFourPlayerSkirmishStrategicSample> M_StrategicSamples;
	float M_LastUnitCountQueryFightSeconds = -1.f;
	int32 M_NextUnitCountQueryId = 1;
	TArray<FFourPlayerSkirmishOwnerChangeCase> M_OwnerChangeCases;
	TArray<FString> M_OwnerChangeLines;
	TArray<FString> M_OwnerChangeFailReasons;
	// Filled once when the fight starts and never resized after: wave callbacks address waves by index.
	TArray<FFourPlayerSkirmishControllerWave> M_ControllerWaves;
	// FAIL reasons of the controller checks; they run beside the fight and join the verdict at the end.
	TArray<FString> M_ControllerFailReasons;
	// Registry findings for the summary (map controllers, the argument-less getter).
	TArray<FString> M_ControllerLines;
	FDelegateHandle M_DamageAppliedHandle;
	FVector M_ArenaCentre = FVector::ZeroVector;
	FString M_OutputDirectory;
	FString M_HitsCsvPath;
	FString M_SummaryPath;
	FString M_FightEndReason;
	float M_SquareSideCm = 0.f;
	float M_MaxFightGameSeconds = 0.f;
	double M_StartRealSeconds = 0.0;
	float M_FightStartGameSeconds = 0.f;
	float M_LastSampleGameSeconds = 0.f;
	float M_LastStatusLogFightSeconds = 0.f;
	// Fight seconds since which the criteria hold without a break; negative while they do not hold.
	float M_CriteriaHeldSinceSeconds = -1.f;
	// Fight seconds at which at most one player still had living units; negative while more are alive.
	float M_LastPlayerStandingSeconds = -1.f;
	bool bM_IsActive = false;
	bool bM_IsSetupGateDone = false;
	bool bM_IsSetupTimedOut = false;
	bool bM_IsPhaseArgumentInvalid = false;

#endif
};
