// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/SurvivalMode/Core/SurvivalModeTypes.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveComposer.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveTypes.h"

#include "SurvivalWaveDirectorComponent.generated.h"

class AAircraftMaster;
class AEnemyController;
class ASquadController;
struct FAttackMoveWaveSettings;
struct FStreamableHandle;
class ASurvivalEdgeSpawnZone;
class ASurvivalModeDirector;
class ASquadUnit;
class ATankMaster;
class ATankMaster;
class USurvivalStrategicAICoordinator;
class USurvivalWaveMarkerTracker;
class USurvivalWaveSettings;
struct FFormationData;

/**
 * @brief One formation launched by the wave director, tracked until it reaches the base (then its survivors
 * belong to the strategic AI) so it can be re-aimed when the player's base moves.
 */
USTRUCT()
struct FSurvivalTrackedWaveGroup
{
	GENERATED_BODY()

	int32 WaveNumber = 0;

	// Identifies the formation in the formation controller: its last waypoint is exactly this point.
	FVector ExpectedFinalWaypoint = FVector::ZeroVector;

	// Base location the group was last aimed at; a retarget happens once the base moved away from it.
	FVector AimedBaseLocation = FVector::ZeroVector;

	double LaunchTime = 0.0;
	double LastRetargetTime = -1.0e9;
	int32 PlannedUnitCount = 0;
	float PlannedLoad = 0.f;
	int32 FormationID = INDEX_NONE;
	bool bFormationSeen = false;
	// Waypoint progress: a formation waits for every unit, so one stuck unit can park the whole group.
	int32 LastWaypointIndex = INDEX_NONE;
	double LastProgressTime = 0.0;
};

USTRUCT()
struct FSurvivalRecentEdgeUse
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ASurvivalEdgeSpawnZone> Zone;

	int32 WaveNumber = 0;
};

/**
 * @brief One transport of a heavy airlift and the tank it carries.
 */
USTRUCT()
struct FSurvivalAirliftSlot
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> Aircraft;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> Tank;

	FVector DropLocation = FVector::ZeroVector;
	bool bReleased = false;
	// Shot down, failed to spawn or died before it was released: no longer waited for.
	bool bLost = false;
};

/**
 * @brief Heavy tanks flown from the zone across the map to its centre. Once they landed they become one tracked
 * attack-move group like the ground waves (retargeting, minimap marker, round attribution).
 */
USTRUCT()
struct FSurvivalAirliftRun
{
	GENERATED_BODY()

	int32 RunId = 0;

	UPROPERTY()
	TArray<FSurvivalAirliftSlot> Slots;

	FVector DropCenter = FVector::ZeroVector;
	// Chosen at launch so the minimap marker can bind to the formation the run becomes.
	FVector FinalWaypoint = FVector::ZeroVector;
	FVector AimedBaseLocation = FVector::ZeroVector;
	double LaunchTime = 0.0;
	double FirstReleaseTime = -1.0;
	double LastReleaseTime = -1.0;
	bool bFormed = false;
};

/**
 * @brief Scheduling state of the running waves.
 */
USTRUCT()
struct FSurvivalWaveScheduleState
{
	GENERATED_BODY()

	bool bRunning = false;
	bool bWaitingForUnitCap = false;
	double WavesStartWorldTime = -1.0;
	double NextWaveWorldTime = 0.0;
	double NextGroupUpdateWorldTime = 0.0;
	int32 WavesSinceSpecial = 0;
	int32 LastMilestoneIndex = 0;
	EEnemyAITechLevel CurrentTechStage = EEnemyAITechLevel::NotInitialized;
};

/**
 * @brief Everything decided for one wave before its formations are created.
 */
struct FSurvivalWavePlan
{
	const FSurvivalWaveKindDefinition* KindDefinition = nullptr;
	TArray<FSurvivalPlannedWaveUnit> Units;
	TArray<ASurvivalEdgeSpawnZone*> Edges;
	FVector BaseLocation = FVector::ZeroVector;
	float Minute = 0.f;
	int32 Budget = 0;
	int32 GlobalAbilitiesCast = 0;
	// Heavy tanks flown in from the far side with this wave group (0 = none) and where they land.
	int32 AirliftTanks = 0;
	FVector AirliftDropCenter = FVector::ZeroVector;
};

/**
 * @brief Launches the Soviet waves of a survival match from the map edges at the player's current base.
 * Waves grow with the survival clock per difficulty (USurvivalWaveSettings); each wave is split over one or more
 * ASurvivalEdgeSpawnZones, spawned through the level's AEnemyController (UEnemyWaveController single attack-move
 * waves) and re-aimed while en route when the base moves. Survivors that reach the base are handed to the
 * strategic AI by the formation controller; USurvivalStrategicAICoordinator configures that AI.
 * @note Owned by ASurvivalModeDirector; the four contract functions below are called by it. The HUD binds
 * OnWaveIncoming / OnTechStageReached and polls GetSecondsUntilNextWave.
 */
UCLASS(ClassGroup=(Survival), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API USurvivalWaveDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USurvivalWaveDirectorComponent();

	/** @brief Called by the director at BeginPlay. */
	void InitWaves(ASurvivalModeDirector* Director);

	/** @brief Called when the grace period ends. */
	void StartWaves();

	/**
	 * @brief Called by the director once the player picked a start: the zone across the map becomes the heavy
	 * airlift origin (no ground waves) and the other zones slide along their rim toward the start.
	 * @param StartLocation Deploy centre of the chosen start.
	 */
	void ConfigureEdgesForStart(const FVector& StartLocation);

	/** @return The zone whose share of the waves arrives by air; null when the airlift is off. */
	ASurvivalEdgeSpawnZone* GetAirliftOriginZone() const { return M_AirliftOriginZone.Get(); }

	/** @brief Called on defeat. */
	void StopWaves();

	/** @return The number of the current wave (round); 0 before the first one. */
	int32 GetWavesLaunched() const { return M_RoundNumber; }

	/** @return Rounds fully survived. */
	int32 GetRoundsSurvived() const { return M_RoundsSurvived; }

	ESurvivalRoundState GetRoundState() const { return M_RoundState; }

	/**
	 * @return Enemies of the current round still alive (a squad counts once) plus those still spawning; 0 between
	 * rounds.
	 */
	int32 GetEnemiesRemaining() const;

	/** @return Seconds the current round keeps sending groups; 0 once it is clearing. */
	float GetRoundActiveSecondsRemaining() const;

	/** @brief Called by the survival director when the intermission after a survived round is over. */
	void StartNextRound();

	/** @brief Broadcast when a round is survived; the director starts the intermission and offers rewards. */
	FOnSurvivalRoundSurvived OnRoundSurvived;

	/**
	 * @brief Broadcast when a wave launches, WarningLeadSeconds before its units appear, with the wave number,
	 * kind, edge names/directions, unit count and a ready-made "Wave N incoming from <edges>" message.
	 */
	FOnSurvivalWaveIncoming OnWaveIncoming;

	/** @brief Broadcast when the survival clock unlocks a new Soviet tech stage. */
	FOnSurvivalTechStageReached OnTechStageReached;

	/** @return Seconds until the next wave launches; 0 while waiting for the unit cap or when not running. */
	float GetSecondsUntilNextWave() const;

	/** @return 1-based number the next wave will get. */
	int32 GetNextWaveNumber() const { return M_WavesLaunched + 1; }

	EEnemyAITechLevel GetCurrentTechStage() const { return M_Schedule.CurrentTechStage; }

	/** @return True while a due wave waits because too many Soviet units are alive. */
	bool GetIsWaitingForUnitCap() const { return M_Schedule.bWaitingForUnitCap; }

	bool GetAreWavesRunning() const { return M_Schedule.bRunning; }

	/** @return The last broadcast announcement (for a HUD created after the broadcast). */
	const FSurvivalWaveAnnouncement& GetLastAnnouncement() const { return M_LastAnnouncement; }

	/** @return Current alive Soviet load (tanks + soldiers * SoldierLoadWeight + not yet spawned groups). */
	float GetAliveEnemyLoad() const;

	/** @return The load cap of the current difficulty. */
	float GetAliveEnemyLoadCap() const;

	/** @return The minimap markers of the launched waves; null before the first wave. */
	USurvivalWaveMarkerTracker* GetWaveMarkerTracker() const { return M_WaveMarkerTracker; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<ASurvivalModeDirector> M_Director;

	// Groups launched over the whole match (several per round).
	int32 M_WavesLaunched = 0;

	int32 M_RoundNumber = 0;
	int32 M_RoundsSurvived = 0;
	ESurvivalRoundState M_RoundState = ESurvivalRoundState::Idle;
	double M_RoundActiveEndTime = 0.0;
	double M_RoundClearStartTime = 0.0;

	// Spawn points of one launched group; units appearing there within the window belong to the round.
	struct FRoundSpawnMarker
	{
		TArray<FVector> SpawnPoints;
		int32 Expected = 0;
		int32 Attributed = 0;
		double LaunchTime = 0.0;
	};
	TArray<FRoundSpawnMarker> M_RoundSpawnMarkers;
	TArray<TWeakObjectPtr<AActor>> M_RoundTanks;
	TArray<TWeakObjectPtr<AActor>> M_RoundSquads;
	// Wave units of every round still alive (leftovers included); the alive cap counts these, not the Soviet base
	// garrisons.
	TArray<TWeakObjectPtr<AActor>> M_AllWaveTanks;
	TArray<TWeakObjectPtr<AActor>> M_AllWaveSquads;
	bool bM_LoggedCapWait = false;
	bool bM_HasRegisteredUnitListeners = false;

	void BeginRound();
	void UpdateRound();
	void FinishRound();
	float GetRoundActiveSeconds(int32 RoundNumber) const;
	void RegisterUnitListeners();
	void OnEnemyTankChanged(ATankMaster* Tank, bool bWasAdded);
	// Soldiers register before their squad is assigned, so squads are found by scanning near the spawn markers.
	void AttributeNewSquads();
	bool TryAttributeToRound(const FVector& Location);
	void PruneRoundUnits();
	int32 GetPendingRoundSpawns() const;

	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController;

	// Latch so a map without an enemy controller reports once instead of on every scheduler tick.
	bool bM_ReportedMissingEnemyController = false;

	UPROPERTY()
	TObjectPtr<USurvivalStrategicAICoordinator> M_StrategicAICoordinator;

	// Minimap marker per wave and edge; created with the first launched wave.
	UPROPERTY()
	TObjectPtr<USurvivalWaveMarkerTracker> M_WaveMarkerTracker;

	void MarkLaunchedFormation(const ASurvivalEdgeSpawnZone& Zone, const FVector& FinalWaypoint,
	                           const TArray<FVector>& SpawnPoints, const FVector& BaseLocation);

	UPROPERTY()
	FSurvivalWaveScheduleState M_Schedule;

	UPROPERTY()
	TArray<FSurvivalTrackedWaveGroup> M_TrackedGroups;

	UPROPERTY()
	TArray<FSurvivalRecentEdgeUse> M_RecentEdgeUses;

	UPROPERTY()
	FSurvivalWaveAnnouncement M_LastAnnouncement;

	FRandomStream M_Random;
	FTimerHandle M_SchedulerTimerHandle;
	FDelegateHandle M_PhaseChangedHandle;

	// ---------------- Validity / lookups
	bool GetIsValidDirector() const;
	bool GetIsValidStrategicAICoordinator() const;
	USurvivalWaveSettings* GetWaveSettings() const;
	AEnemyController* GetEnemyController();
	const FSurvivalWaveDifficultyProfile* GetProfile() const;
	float GetSurvivalMinute() const;
	double GetWorldTime() const;

	void OnDirectorPhaseChanged(const ESurvivalPhase OldPhase, const ESurvivalPhase NewPhase);

	// ---------------- Scheduling
	void OnSchedulerTick();
	void UpdateTechStage(const float Minute);
	void ScheduleNextWave(const float Minute);
	void TryLaunchWave();
	const FSurvivalWaveKindDefinition* PickWaveKind(const float Minute, const FSurvivalWaveDifficultyProfile& Profile);
	bool GetIsSpecialWaveAllowed(const FSurvivalWaveKindDefinition& Definition, const float Minute,
	                             const FSurvivalWaveDifficultyProfile& Profile);
	int32 GetMilestoneIndex(const float Minute) const;
	float GetWaveBudget(const FSurvivalWaveKindDefinition& Kind, const float Minute,
	                    const FSurvivalWaveDifficultyProfile& Profile);

	/**
	 * @brief Drops units until the wave fits the alive-load cap.
	 * @param InOutUnits Planned units; the cheapest are removed first.
	 * @return False when the trimmed wave is too small to be worth launching now.
	 */
	bool TrimWaveToUnitCap(TArray<FSurvivalPlannedWaveUnit>& InOutUnits) const;
	float GetPlannedLoad(const TArray<FSurvivalPlannedWaveUnit>& Units) const;

	// ---------------- Edges
	int32 GetEdgeCountForWave(const float Minute, const FSurvivalWaveDifficultyProfile& Profile,
	                          const FSurvivalWaveKindDefinition& Kind);
	TArray<ASurvivalEdgeSpawnZone*> GetEligibleEdges(const float Minute, const FVector& BaseLocation) const;

	/**
	 * @brief Weighted pick of distinct edges; recently used edges are penalised and extra edges prefer
	 * directions away from those already picked.
	 * @param Minute Survival minute for ActiveAfterMinute.
	 * @param BaseLocation Where the wave aims.
	 * @param EdgeCount How many edges are wanted.
	 * @return Picked edges (fewer when not enough are eligible).
	 */
	TArray<ASurvivalEdgeSpawnZone*> PickEdges(const float Minute, const FVector& BaseLocation, const int32 EdgeCount);
	float GetEdgeWeight(const ASurvivalEdgeSpawnZone& Zone, const FVector& BaseLocation,
	                    const TArray<ASurvivalEdgeSpawnZone*>& AlreadyPicked) const;
	bool GetWasEdgeUsedRecently(const ASurvivalEdgeSpawnZone& Zone) const;
	void RememberEdges(const TArray<ASurvivalEdgeSpawnZone*>& Edges);

	/**
	 * @brief Deals the (shuffled) units over the edges round-robin, honouring each zone's MaxUnitsPerWave.
	 * @param Plan Units and edges of the wave.
	 * @param OutUnitsPerEdge One array per edge in Plan.Edges.
	 */
	void DistributeUnitsOverEdges(const FSurvivalWavePlan& Plan,
	                              TArray<TArray<FSurvivalPlannedWaveUnit>>& OutUnitsPerEdge);

	// ---------------- Launching
	bool BuildWavePlan(FSurvivalWavePlan& OutPlan);
	int32 LaunchWavePlan(FSurvivalWavePlan& Plan);

	/**
	 * @brief Spawns the units of one edge as one or more single attack-move waves on the enemy controller.
	 * @param Zone Edge to spawn at.
	 * @param Units Units dealt to this edge.
	 * @param BaseLocation Base to aim at.
	 * @return Units handed to the wave controller.
	 */
	int32 LaunchEdgeGroup(ASurvivalEdgeSpawnZone& Zone, const TArray<FSurvivalPlannedWaveUnit>& Units,
	                      const FVector& BaseLocation);

	/**
	 * @brief Starts one formation-sized chunk through AEnemyController::CreateSingleAttackMoveWave and tracks it.
	 * @param Zone Edge of the chunk.
	 * @param Units The chunk's units.
	 * @param SpawnPoints One navmesh point per unit.
	 * @param BaseLocation Base to aim at.
	 * @return True when the wave request was made.
	 */
	bool LaunchFormationChunk(const ASurvivalEdgeSpawnZone& Zone, const TArray<FSurvivalPlannedWaveUnit>& Units,
	                          const TArray<FVector>& SpawnPoints, const FVector& BaseLocation);
	void EnsureWaveSupplyFor(const int32 UnitCount);
	int32 CastGlobalAbilities(const FSurvivalWaveKindDefinition& Kind, const FVector& BaseLocation);
	bool GetHasAnyReadyGlobalAbility(const FSurvivalWaveKindDefinition& Kind);

	// ---------------- Waypoints
	FVector GetUniqueFinalWaypoint(const FVector& BaseLocation);
	bool GetIsFinalWaypointInUse(const FVector& Candidate) const;

	/**
	 * @brief Nav path from Start to FinalWaypoint resampled into formation waypoints; the last waypoint is
	 * exactly FinalWaypoint (that is how the formation is found again).
	 * @param Start Staging point or the formation's current centre.
	 * @param FinalWaypoint Unique point near the base.
	 * @param bIncludeStart Whether Start is the first waypoint.
	 * @return At least one waypoint.
	 */
	TArray<FVector> BuildWaypoints(const FVector& Start, const FVector& FinalWaypoint, const bool bIncludeStart);
	int32 GetFormationWidth(const int32 UnitCount) const;

	// ---------------- Tracking and retargeting
	void UpdateTrackedGroups();
	int32 FindFormationIndexForGroup(const FSurvivalTrackedWaveGroup& Group,
	                                 const TArray<FFormationData>& Formations) const;

	/**
	 * @brief Re-aims a group at the base's new location: removes its formation and immediately starts a new
	 * attack-move formation with the same units (in one go, so no unit is outside a formation and no death can
	 * leak wave supply).
	 * @param Group Group to re-aim; updated with the new final waypoint.
	 * @param Formation Copy of the group's current formation.
	 * @param BaseLocation The base's current location.
	 * @return False when the formation had no valid units left.
	 */
	bool RetargetGroup(FSurvivalTrackedWaveGroup& Group, const FFormationData& Formation, const FVector& BaseLocation);

	/** @return True when the group's formation has not reached a new waypoint for StalledGroupSeconds. */
	bool UpdateGroupProgress(FSurvivalTrackedWaveGroup& Group, const FFormationData& Formation, const double Now) const;

	/**
	 * @brief Unparks a stalled group: the units that reached the current waypoint continue on a fresh path (a new
	 * formation), the stragglers that hold it back are handed to the strategic AI.
	 * @param Group The stalled group; re-aimed like a retarget.
	 * @param Formation Copy of its formation.
	 * @param BaseLocation The base's current location.
	 * @return False when the formation had no valid units left.
	 */
	bool RecoverStalledGroup(FSurvivalTrackedWaveGroup& Group, const FFormationData& Formation,
	                         const FVector& BaseLocation);

	/**
	 * @brief Extracts the valid squads and tanks of a formation copy.
	 * @param Formation Formation copy from the formation controller.
	 * @param OutSquads Valid squad controllers.
	 * @param OutTanks Valid tanks.
	 * @param OutCentroid Average location of the found units.
	 */
	void GetFormationUnits(const FFormationData& Formation, TArray<ASquadController*>& OutSquads,
	                       TArray<ATankMaster*>& OutTanks, FVector& OutCentroid) const;

	FAttackMoveWaveSettings MakeAttackMoveSettings() const;
	// Survival waves attack-move between waypoints (enemy formation controller switch).
	void EnableUnitAttackMoveForWaves();

	// ---------------- Edge layout (SurvivalWaveDirectorAirlift.cpp)

	// The zone across the map from the start: sends heavy tanks by air instead of ground waves.
	UPROPERTY()
	TWeakObjectPtr<ASurvivalEdgeSpawnZone> M_AirliftOriginZone;

	// Centre of the area the edge zones enclose; the airlift lands around it.
	FVector M_MapCentre = FVector::ZeroVector;

	/**
	 * @brief The enabled zone whose direction from the map centre is most opposite to the start's.
	 * @param Zones Enabled edge zones.
	 * @param StartLocation Deploy centre of the chosen start.
	 * @return Null when there are fewer than two zones.
	 */
	ASurvivalEdgeSpawnZone* FindOppositeZone(const TArray<ASurvivalEdgeSpawnZone*>& Zones,
	                                         const FVector& StartLocation) const;

	/**
	 * @brief Slides a zone along its rim (the axis across its inward direction) toward the start's coordinate on
	 * that axis; shrinks the slide until the spot is on navmesh, far enough from the start and clear of other starts.
	 * @param Zone Zone to move.
	 * @param StartLocation Deploy centre of the chosen start.
	 */
	void SlideZoneTowardStart(ASurvivalEdgeSpawnZone& Zone, const FVector& StartLocation) const;
	bool GetIsSlidCenterAllowed(const FVector& Candidate, const FVector& StartLocation) const;

	// ---------------- Heavy airlift (SurvivalWaveDirectorAirlift.cpp)

	UPROPERTY()
	TArray<FSurvivalAirliftRun> M_AirliftRuns;

	int32 M_NextAirliftRunId = 1;
	double M_LastAirliftTime = -1.0e9;

	// Loaded asynchronously when the start is configured; an airlift is skipped until it is in memory.
	UPROPERTY()
	TObjectPtr<UClass> M_AirliftAircraftClass = nullptr;

	TSharedPtr<FStreamableHandle> M_AirliftAircraftLoadHandle;

	void RequestAirliftAircraftClass();
	void OnAirliftAircraftClassLoaded();
	bool GetIsAirliftReady() const;

	/**
	 * @brief Flies heavy tanks from the far-side zone to the map centre with this wave group when one is due.
	 * @param InOutPlan The launched plan; receives the airlift tank count and drop centre.
	 */
	void TryLaunchAirlift(FSurvivalWavePlan& InOutPlan);

	int32 GetAirliftTankCount(const float Minute) const;

	/**
	 * @brief Weighted picks among the unlocked pool entries with an airlift role.
	 * @param Minute Survival minute (unlock, ramp and fade).
	 * @param Count Tanks wanted.
	 * @param OutUnits Picked units; empty when no airlift role is unlocked yet.
	 */
	void PickAirliftUnits(const float Minute, const int32 Count, TArray<FSurvivalPlannedWaveUnit>& OutUnits);

	/**
	 * @brief A navmesh point at (or on rings around) the map centre with a vehicle path to the base.
	 * @param BaseLocation Where the tanks will go.
	 * @param OutDropCenter The chosen point.
	 * @return False when no candidate projects to the navmesh.
	 */
	bool FindAirliftDropCenter(const FVector& BaseLocation, FVector& OutDropCenter) const;
	bool GetHasVehiclePath(const FVector& From, const FVector& To) const;
	TArray<FVector> BuildAirliftDropLocations(const FVector& DropCenter, const int32 Count) const;

	/**
	 * @brief Spawns one transport over the origin zone and its tank; the drop is ordered once the tank exists.
	 * @param Run The airlift (its slot array is already sized).
	 * @param SlotIndex Slot of this transport.
	 * @param Unit Tank to carry.
	 * @param AircraftSpawnLocation Where the transport appears.
	 * @param RetreatLocation Where it flies after the drop.
	 * @return True when the tank spawn was requested.
	 */
	bool LaunchAirliftSlot(FSurvivalAirliftRun& Run, const int32 SlotIndex, const FSurvivalPlannedWaveUnit& Unit,
	                       const FVector& AircraftSpawnLocation, const FVector& RetreatLocation);
	void OnAirliftTankSpawned(const int32 RunId, const int32 SlotIndex, AActor* SpawnedActor,
	                          const FVector& RetreatLocation);
	void OnAirliftPayloadReleased(AAircraftMaster* Aircraft, ATankMaster* ReleasedTank);

	void UpdateAirliftRuns();
	void UpdateAirliftSlotStates(FSurvivalAirliftRun& Run) const;
	bool GetShouldFormAirliftGroup(const FSurvivalAirliftRun& Run, const double Now) const;
	void FormAirliftGroup(FSurvivalAirliftRun& Run);
	FSurvivalAirliftRun* FindAirliftRun(const int32 RunId);
	// Load of airlift tanks that do not exist yet; the alive cap counts them like unspawned ground groups.
	float GetPendingAirliftLoad() const;

	// Map-test runs (-SurvivalWaveProbe): logs how wave units are ordered (attack move, plain move, attacking).
	void LogWaveProbeIfRequested();
	double M_NextWaveProbeTime = 0.0;

	// Map-test runs (-SurvivalDecoyTest): one player tank put on a wave group's path proves the group stops to fight.
	void UpdateDecoyTest();
	struct FSurvivalDecoyStats
	{
		TArray<TWeakObjectPtr<AActor>> NearAtSpawn;
		double SpawnTime = 0.0;
		double KilledAfterSeconds = -1.0;
		int32 MaxAttacking = 0;
		int32 StillNearAt20s = -1;
		int32 AliveSamples = 0;
		float SumAttacking = 0.f;
		float SumNear = 0.f;
		bool bSpawned = false;
	};
	FSurvivalDecoyStats M_DecoyStats;
	TArray<int32> M_DecoyFormationIDs;
	int32 M_DecoyCount = 0;
	UPROPERTY()
	TWeakObjectPtr<AActor> M_DecoyTank;
	FVector M_DecoyLocation = FVector::ZeroVector;
	bool bM_DecoyRequested = false;

	// ---------------- Announcement
	void BroadcastAnnouncement(const FSurvivalWavePlan& Plan, const int32 LaunchedUnits);
	FText BuildAnnouncementMessage(const FSurvivalWaveAnnouncement& Announcement) const;
	FText JoinEdgeNames(const TArray<FText>& EdgeNames) const;

	void StopSchedulerTimer();
};
