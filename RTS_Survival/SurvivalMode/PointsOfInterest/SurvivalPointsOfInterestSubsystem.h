// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOISeating.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOITypes.h"

#include "SurvivalPointsOfInterestSubsystem.generated.h"

class ASurvivalModeDirector;
class ASurvivalPOIPCGVolume;
class UNiagaraComponent;
class UPrimitiveComponent;
class FSurvivalGroundProjector;
class FSurvivalPOIPlanner;
class USurvivalPointsOfInterestSettings;
struct FStreamableHandle;

/** @brief One spawned object of a point of interest. */
struct FSurvivalPOISpawnRecord
{
	FSurvivalPOIPlacedObject Planned;
	TWeakObjectPtr<AActor> Actor;
	FSurvivalPOISeatReport Seat;
	// The transform right after seating; the census compares the live actor with it.
	FTransform SpawnedTransform = FTransform::Identity;
};

/** @brief One spawned herd or flock. */
struct FSurvivalWildlifeRecord
{
	FSurvivalWildlifeGroup Group;
	TArray<TWeakObjectPtr<AActor>> Actors;
	// World time before which the herd ignores nearby units again (it just bolted).
	double AlarmCooldownUntil = 0.0;
};

/**
 * @brief A spawned animal or effect-carrying prop that sleeps while nobody sees it: animals stop their actor tick
 * (flock instance updates, walking), props pause their Niagara effects. It wakes as soon as any visible part renders.
 */
struct FSurvivalPOIOffscreenSleeper
{
	TWeakObjectPtr<AActor> Actor;
	// The parts whose on-screen render time decides; never the paused effects themselves.
	TArray<TWeakObjectPtr<UPrimitiveComponent>> VisibleParts;
	TArray<TWeakObjectPtr<UNiagaraComponent>> Effects;
	// Which effects ticked when spawned, so waking restores exactly that.
	TArray<bool> EffectTicked;
	bool bSleepsActorTick = false;
	bool bAwake = true;
};

/** @brief How long generation took and its worst single step, for the perf note. */
struct FSurvivalPOIGenerationStats
{
	double StartSeconds = 0.0;
	double SitesChosenSeconds = 0.0;
	double FinishedSeconds = 0.0;
	double WorstSiteStepMs = 0.0;
	double WorstSpawnStepMs = 0.0;
	double ComposeMs = 0.0;
	double WildlifePlanMs = 0.0;
	int32 SiteSteps = 0;
	int32 SpawnSteps = 0;
	int32 FailedLoads = 0;
	int32 FailedSpawns = 0;
};

UENUM()
enum class ESurvivalPOIGenerationState : uint8
{
	Idle,
	SearchingSites,
	Loading,
	Spawning,
	Finished,
	Failed
};

/**
 * @brief Generates the survival map's points of interest (small battlefields, ruined hangars with capturable vehicles,
 * the sector landmarks) and its wildlife once per match, through the PCG graph in the POI settings asset.
 * The director calls GenerateForMatch once the start location is confirmed; the graph's nodes call back into this
 * subsystem for the plan and the spawning. Everything it spawns carries the actor tag SurvivalPOI.
 */
UCLASS()
class RTS_SURVIVAL_API USurvivalPointsOfInterestSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static const FName SpawnedTag;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/** @brief The one call from the director: finds the world's subsystem and starts this match's generation. */
	static void StartForMatch(ASurvivalModeDirector* Director);

	/** @brief Starts the graph for this match (once; later calls do nothing). */
	void GenerateForMatch(ASurvivalModeDirector* Director);

	// --- Graph nodes ---

	/** @return True when the sites are chosen; until then the sites node calls again next frame. */
	bool StepSiteSearch();
	const TArray<FSurvivalPOISite>& GetPlannedSites() const;
	void ComposeSites(const TArray<FSurvivalPOISite>& Sites, TArray<FSurvivalPOIPlacedObject>& OutObjects);
	void PlanWildlife(const TArray<FSurvivalPOISite>& Sites, TArray<FSurvivalWildlifeGroup>& OutGroups);
	bool GetIsSpawnRequested() const { return bM_SpawnRequested; }

	/**
	 * @brief Loads every asset the points name, spawns them in batches over frames and seats them.
	 * @param OnFinished Wakes the sleeping spawn node when everything stands.
	 */
	void SpawnFromGraph(TArray<FSurvivalPOIPlacedObject>&& Objects, TArray<FSurvivalWildlifeGroup>&& Groups,
	                    TFunction<void()> OnFinished);

	// --- Queries (preview, census, manifests) ---

	ESurvivalPOIGenerationState GetState() const { return M_State; }
	bool GetIsFinished() const { return M_State == ESurvivalPOIGenerationState::Finished; }
	const USurvivalPointsOfInterestSettings* GetSettings() const { return M_Settings; }
	const FSurvivalPOIPlanner* GetPlanner() const { return M_Planner.Get(); }
	FSurvivalPOIPlanner* GetMutablePlanner() { return M_Planner.Get(); }
	const TArray<FSurvivalPOISite>& GetSites() const { return M_Sites; }
	const TArray<FSurvivalPOISpawnRecord>& GetSpawnRecords() const { return M_SpawnRecords; }
	const TArray<FSurvivalWildlifeRecord>& GetWildlifeRecords() const { return M_WildlifeRecords; }
	const FSurvivalPOIGenerationStats& GetStats() const { return M_Stats; }
	int32 GetMatchSeed() const { return M_MatchSeed; }
	ASurvivalModeDirector* GetDirector() const { return M_Director.Get(); }
	const FSurvivalGroundProjector* GetProjector() const { return M_Projector.Get(); }
	/** @return How many off-screen sleepers are awake now, and how many there are (perf note). */
	FIntPoint GetOffscreenSleeperCounts() const;
	/** @brief Someone else switched every tick back on (the perf test): count all sleepers awake, the next check sleeps the unseen. */
	void MarkOffscreenSleepersAwake();

	/** @return The settings asset, or a transient copy with the code defaults when the asset is missing. */
	static USurvivalPointsOfInterestSettings* LoadSettings(UObject* Outer);

private:
	ESurvivalPOIGenerationState M_State = ESurvivalPOIGenerationState::Idle;

	UPROPERTY()
	TObjectPtr<USurvivalPointsOfInterestSettings> M_Settings = nullptr;

	UPROPERTY()
	TObjectPtr<ASurvivalPOIPCGVolume> M_Volume = nullptr;

	TWeakObjectPtr<ASurvivalModeDirector> M_Director;
	// Shared pointers: their deleters are bound where they are made, so the forward declarations above suffice.
	TSharedPtr<FSurvivalPOIPlanner> M_Planner;
	TSharedPtr<FSurvivalGroundProjector> M_Projector;
	int32 M_MatchSeed = 0;
	bool bM_SpawnRequested = false;

	// The sites the compose node received: what is built, after any graph filter.
	TArray<FSurvivalPOISite> M_Sites;
	TArray<FSurvivalPOIPlacedObject> M_PendingObjects;
	TArray<FSurvivalWildlifeGroup> M_PendingGroups;
	int32 M_NextObject = 0;
	int32 M_NextGroup = 0;
	TArray<FSurvivalPOISpawnRecord> M_SpawnRecords;
	TArray<FSurvivalWildlifeRecord> M_WildlifeRecords;
	TSharedPtr<FStreamableHandle> M_LoadHandle;
	TFunction<void()> M_OnSpawnFinished;
	FTimerHandle M_SpawnTimer;
	FTimerHandle M_AlarmTimer;
	FTimerHandle M_SleepTimer;
	FSurvivalPOIGenerationStats M_Stats;
	TArray<FSurvivalPOIOffscreenSleeper> M_Sleepers;

	int32 ResolveMatchSeed() const;
	void StartGraph();
	void OnAssetsLoaded();
	void SpawnStep();
	void FinishSpawning();

	AActor* SpawnObject(const FSurvivalPOIPlacedObject& Object, FSurvivalPOISeatReport& OutSeat);
	AActor* SpawnDecal(const FSurvivalPOIAsset& Asset, const FSurvivalPOIPlacedObject& Object);
	AActor* SpawnMeshProp(const FSurvivalPOIAsset& Asset, const FSurvivalPOIPlacedObject& Object);
	AActor* SpawnActorClass(const FSurvivalPOIAsset& Asset, const FSurvivalPOIPlacedObject& Object);
	void SpawnWildlifeGroup(const FSurvivalWildlifeGroup& Group);
	void SpawnHerd(const FSurvivalWildlifeEntry& Entry, const FSurvivalWildlifeGroup& Group, FSurvivalWildlifeRecord& OutRecord);
	void SpawnBirdFlock(const FSurvivalWildlifeEntry& Entry, const FSurvivalWildlifeGroup& Group, FSurvivalWildlifeRecord& OutRecord);
	void SpawnNiagaraFlock(const FSurvivalWildlifeEntry& Entry, const FSurvivalWildlifeGroup& Group, FSurvivalWildlifeRecord& OutRecord);
	void MarkSpawned(AActor& Actor) const;
	void WarnIfLayoutStale() const;
#if RTS_WITH_MAP_TESTS
	void StartFrameWatch();
#endif
	void ThrottlePropTicks(AActor& Actor) const;

	/** @brief Collects the animals and effect-carrying props that may sleep off screen and starts the wake check. */
	void StartOffscreenSleep();
	void AddOffscreenSleeper(AActor& Actor, bool bSleepsActorTick);
	void UpdateOffscreenSleepers();
	static bool GetIsSleeperOnScreen(const FSurvivalPOIOffscreenSleeper& Sleeper, const UWorld& World);
	static void SetSleeperAwake(FSurvivalPOIOffscreenSleeper& Sleeper, bool bAwake);

	/** @brief Herds bolt from any unit that comes within their alarm radius (the Wanderer itself only flees when hit). */
	void CheckWildlifeAlarms();
	TOptional<FVector> FindThreatNearHerd(const FSurvivalWildlifeRecord& Record, float Radius) const;

	bool GetIsValidSettings() const;
	bool GetIsValidPlanner() const;
};
