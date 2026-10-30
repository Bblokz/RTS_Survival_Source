// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/TimerHandle.h"
#include "UObject/ObjectKey.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseLayout.h"

#include "SurvivalSovietBaseComponent.generated.h"

class ASurvivalModeDirector;
class ASurvivalStartLocation;
class USurvivalSovietBaseSettings;
struct FStreamableHandle;

/** @brief Broadcast once when all production buildings of a base are dead (BaseIndex, base centre). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSurvivalSovietBaseDestroyed, int32 /*BaseIndex*/, const FVector& /*BaseCenter*/);

/**
 * @brief Runtime state of one generated Soviet base.
 */
struct FSurvivalSovietBaseState
{
	int32 BaseIndex = INDEX_NONE;
	TWeakObjectPtr<ASurvivalStartLocation> StartLocation;
	int32 PresetIndex = INDEX_NONE;
	int32 Strength = 0;
	int32 Seed = 0;
	FSurvivalSovietBaseDifficultyScaling Scaling;

	FVector Center = FVector::ZeroVector;
	FVector LaunchPoint = FVector::ZeroVector;
	FSurvivalSovietBasePlan Plan;

	TArray<TWeakObjectPtr<AActor>> SpawnedStructures;
	TArray<TWeakObjectPtr<AActor>> GarrisonUnits;
	// Ground traces ignore the structures this base already spawned.
	FCollisionQueryParams GroundTraceParams;

	// Structures whose death counts towards destroying the base (production, or every building when the base
	// has no production building).
	int32 TrackedStructuresAlive = 0;
	int32 TrackedStructuresTotal = 0;
	int32 PendingStructureSpawns = 0;
	int32 PendingGarrisonGroups = 0;

	TSharedPtr<FStreamableHandle> LoadHandle;
	bool bPlanBuilt = false;
	bool bStructuresSpawned = false;
	bool bDestroyed = false;
};

/**
 * @brief Generates a Soviet base on every start location the player did not pick.
 *
 * Flow per base: pick strength (distance rank from the player + difficulty) and a preset (seeded) -> async load
 * the preset's classes -> measure footprints -> FSurvivalSovietBaseLayout plans production compound, perimeter
 * lines, gun pits, support ring, props and garrison with the shared EnemyBaseSetup placement builder -> structures
 * spawn in small batches over frames -> garrison spawns through ARTSAsyncSpawner.
 * Spawned Rus building expansions register with the game unit manager as enemy (player 2) in their own BeginPlay,
 * which unlocks enemy tech levels and makes the strategic AI see the base as an AI base cluster.
 * @note Owned by the Soviet-base design; the public signatures of InitSovietBases and GenerateSovietBaseAt are
 * part of the contract.
 */
UCLASS(ClassGroup=(Survival), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API USurvivalSovietBaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USurvivalSovietBaseComponent();

	/** @brief Called by the director at BeginPlay. */
	void InitSovietBases(ASurvivalModeDirector* Director);

	/** @brief Builds a base on the start location; BaseIndex is 0..N-1 over the unpicked locations. */
	void GenerateSovietBaseAt(ASurvivalStartLocation* StartLocation, int32 BaseIndex);

	/** @brief Fired when a base is destroyed; the waves director and the HUD listen to it. */
	FOnSurvivalSovietBaseDestroyed OnSovietBaseDestroyed;

	/** @return Bases requested so far (destroyed ones included). */
	UFUNCTION(BlueprintPure, Category = "Survival|Soviet Bases")
	int32 GetSovietBaseCount() const { return M_Bases.Num(); }

	/** @return Bases that still have a production building alive. */
	UFUNCTION(BlueprintPure, Category = "Survival|Soviet Bases")
	int32 GetAliveSovietBaseCount() const;

	/** @return Centres of the bases that are still alive. */
	TArray<FVector> GetAliveBaseCenters() const;

	/**
	 * @return Per alive base a ground point on its perimeter facing the player's start location; a natural
	 * spawn point for attacks that come out of a base.
	 */
	TArray<FVector> GetAliveBaseLaunchPoints() const;

	/** @return 1 + alive bases * WavePressurePerAliveBase; the waves director may scale wave budgets with it. */
	UFUNCTION(BlueprintPure, Category = "Survival|Soviet Bases")
	float GetWavePressureMultiplier() const;

	/** @return Strength (0 = weakest) of the base, -1 when the index is unknown. */
	int32 GetBaseStrength(int32 BaseIndex) const;

	bool GetIsBaseDestroyed(int32 BaseIndex) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<ASurvivalModeDirector> M_Director;

	TArray<FSurvivalSovietBaseState> M_Bases;

	// Per-match seed every base seed is derived from.
	int32 M_MatchSeed = 0;
	bool bM_HasMatchSeed = false;

	// Resolved class table shared by every base; the layout refers to classes by index.
	UPROPERTY()
	TArray<TObjectPtr<UClass>> M_ResolvedClasses;

	TArray<FBox> M_MeasuredLocalBounds;
	TMap<FSoftObjectPath, int32> M_ClassIndexByPath;
	// Classes already reported for not being enemy owned (one error per class).
	TSet<FName> M_ReportedOwnerClassNames;

	/** @brief One structure waiting in the spawn queue. */
	struct FPendingStructureSpawn
	{
		int32 BaseArrayIndex = INDEX_NONE;
		int32 StructureIndex = INDEX_NONE;
	};

	TArray<FPendingStructureSpawn> M_SpawnQueue;
	int32 M_SpawnQueueHead = 0;
	FTimerHandle M_SpawnTimerHandle;

	/** @brief A garrison group whose units are still spawning. */
	struct FPendingGarrisonGroup
	{
		int32 BaseArrayIndex = INDEX_NONE;
		int32 PlannedGroupIndex = INDEX_NONE;
		int32 PendingUnits = 0;
		TArray<TWeakObjectPtr<AActor>> SpawnedUnits;
	};

	TMap<int32, FPendingGarrisonGroup> M_PendingGarrisonGroups;
	int32 M_NextGarrisonGroupId = 0;

	// Structure -> base array index for every structure whose death counts towards destroying its base.
	TMap<TObjectKey<AActor>, int32> M_TrackedStructureToBase;

	bool bM_HasConfiguredStrategicAI = false;

	// ---- Generation ----
	const USurvivalSovietBaseSettings* GetSovietBaseSettings() const;
	bool GetIsValidDirector() const;
	void EnsureMatchSeed(const USurvivalSovietBaseSettings& Settings);
	TArray<ASurvivalStartLocation*> GetUnpickedStartLocations() const;
	int32 ComputeBaseStrength(const ASurvivalStartLocation& StartLocation, int32 BaseIndex,
	                          const USurvivalSovietBaseSettings& Settings,
	                          const FSurvivalSovietBaseDifficultyScaling& Scaling) const;
	int32 PickPresetIndex(const USurvivalSovietBaseSettings& Settings, int32 Strength, int32 Seed) const;
	int32 ComputeBaseSeed(const ASurvivalStartLocation& StartLocation, int32 BaseIndex) const;
	TArray<FSoftObjectPath> CollectPresetClassPaths(const FSurvivalSovietBasePreset& Preset) const;

	void OnBaseAssetsLoaded(int32 BaseArrayIndex);
	void BuildBasePlan(FSurvivalSovietBaseState& Base, const USurvivalSovietBaseSettings& Settings);

	// ---- Footprint resolution ----
	bool ResolveEntry(const FEnemyBlueprintEntry& Entry, FEnemyResolvedEntry& OutResolved);
	int32 FindOrResolveClass(const TSoftClassPtr<AActor>& SoftClass);
	FBox MeasureClassLocalBounds(UClass* ActorClass) const;

	// ---- Structure spawning ----
	void EnqueueBaseStructures(int32 BaseArrayIndex);
	void ProcessSpawnQueue();
	AActor* SpawnStructure(FSurvivalSovietBaseState& Base, const FSurvivalSovietPlannedStructure& Structure);
	FVector ProjectToGround(const FSurvivalSovietBaseState& Base, const FVector2D& Position) const;
	void OnStructureSpawned(int32 BaseArrayIndex, const FSurvivalSovietPlannedStructure& Structure, AActor* Actor);
	void OnBaseStructuresSpawned(int32 BaseArrayIndex);
	/** @brief Reports (once per class) a spawned RTS unit that is not owned by the enemy player. */
	void ValidateEnemyOwnership(AActor* Actor);

	// ---- Destruction tracking ----
	void TrackStructureForDestruction(int32 BaseArrayIndex, AActor* Actor);

	UFUNCTION()
	void OnTrackedStructureDestroyed(AActor* DestroyedActor);

	void OnTrackedStructureLost(const TObjectKey<AActor>& StructureKey);
	void OnBaseDestroyed(int32 BaseArrayIndex);
	void GrantBaseRewards(const FSurvivalSovietBaseState& Base, const FSurvivalSovietBasePreset& Preset) const;

	// ---- Garrison ----
	void SpawnBaseGarrison(int32 BaseArrayIndex);
	void OnGarrisonUnitSpawned(int32 GroupId, AActor* SpawnedActor);
	void OnGarrisonGroupComplete(const FPendingGarrisonGroup& Group);
	void RegisterGarrisonAsStrategicReserve(const TArray<TWeakObjectPtr<AActor>>& Units) const;
	void StartGarrisonPatrol(const FSurvivalSovietBaseState& Base, const FSurvivalSovietPlannedGarrison& Planned,
	                         const TArray<TWeakObjectPtr<AActor>>& Units) const;

	// ---- Strategic AI ----
	void ConfigureStrategicAIBaseDetection(const USurvivalSovietBaseSettings& Settings,
	                                       const FSurvivalSovietBasePlan& Plan);
};
