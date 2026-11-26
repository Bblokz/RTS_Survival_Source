// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "EnemyWaveSpawnerTypes.h"

#include "EnemyWaveSpawner.generated.h"

class AEnemyController;
class AEnemyWaveSpawner;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyWaveSpawnerUnitsSpawned, AEnemyWaveSpawner*, Spawner,
                                             const TArray<AActor*>&, Units);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyWaveSpawnerFinished, AEnemyWaveSpawner*, Spawner);

/**
 * @brief Placed in a mission map to author an enemy attack or patrol wave in the level instead of in mission logic.
 * Drag the SpawnPoints and Waypoints diamonds in the viewport; red = attack wave, blue = patrol wave. The wave runs
 * on the enemy controller's wave controller (wave supply, owning building, formations, direct-control handoff).
 * Nothing of it shows in game. Starts on BeginPlay, or from a mission via FindEnemyWaveSpawner + StartSpawner
 * (a strongpoint can start it as its reinforcements).
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AEnemyWaveSpawner : public AEnemyPlacementActor
{
	GENERATED_BODY()

public:
	AEnemyWaveSpawner();

	/** @brief Starts the timer to the first wave; does nothing while running. After it finished it runs again. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Wave Spawner")
	void StartSpawner();

	/** @brief Stops further waves; units already spawned keep their orders (and minimap markers). */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Wave Spawner")
	void StopSpawner();

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Wave Spawner")
	bool GetIsSpawnerRunning() const { return bM_IsRunning; }

	// Waves that spawned units so far.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Wave Spawner")
	int32 GetSpawnedWaveCount() const { return M_SpawnedWaveCount; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Wave Spawner")
	FName GetSpawnerId() const { return SpawnerId; }

	/**
	 * @brief Replaces the units of the following waves, so a mission can feed them from its per-difficulty data asset
	 * instead of the placed actor's lists. Call it before StartSpawner.
	 * @param NewUnits The lists to use; difficulty extras still add up on top of BaseUnits.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Wave Spawner")
	void SetUnits(const FEnemyWaveSpawnerUnits& NewUnits);

	/** @return Units this spawner spawned that are still alive (squads with men left, vehicles not destroyed). */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Wave Spawner")
	int32 GetLivingSpawnedUnitCount() const;

	/**
	 * @brief Patrol waves only: every living group of this spawner, minus units running to the fallback rally point,
	 * attack-moves to AlarmLocation and returns to its patrol loop CombatSecondsBeforeReturn after its first fight, or
	 * 30 s after arriving when it met nobody. A second call re-targets: groups go to the new location, timers restart.
	 * @param AlarmLocation Where the groups attack-move to (where the alarm unit was seen, or the player HQ).
	 * @param CombatSecondsBeforeReturn Seconds after a group first fights before it breaks off.
	 * @param CombatCheckDelaySeconds Seconds after the order in which neither combat nor arrival starts the return
	 * (time to reach a far target such as the HQ); 0 counts the first fight on the way.
	 * @return Units sent; 0 for an attack wave or a spawner without living patrol units.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Wave Spawner")
	int32 ConvergeOnAlarm(const FVector AlarmLocation, const float CombatSecondsBeforeReturn = 20.f,
	                      const float CombatCheckDelaySeconds = 0.f);

	/**
	 * @brief Sends the patrols of every patrol-wave spawner in the world to an alarm (ConvergeOnAlarm on each).
	 * @param WorldContextObject Any object in the mission world.
	 * @param AlarmLocation Where the groups attack-move to.
	 * @param CombatSecondsBeforeReturn Seconds after a group first fights before it breaks off.
	 * @param CombatCheckDelaySeconds See ConvergeOnAlarm.
	 * @return Units sent over all patrol spawners.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Wave Spawner",
		meta = (WorldContext = "WorldContextObject"))
	static int32 ConvergeAllPatrolsOnAlarm(const UObject* WorldContextObject, const FVector AlarmLocation,
	                                       const float CombatSecondsBeforeReturn = 20.f,
	                                       const float CombatCheckDelaySeconds = 0.f);

	// Strongpoints warn when a spawner linked as their reinforcements also starts by itself.
	bool GetStartsOnBeginPlay() const { return Timing.bStartOnBeginPlay; }

	virtual FColor GetPlacementColor() const override;
	virtual FString GetConfigurationProblem() const override;

	/** @return The placed spawner with this SpawnerId, or null (reports an error) when there is none. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Wave Spawner",
		meta = (WorldContext = "WorldContextObject"))
	static AEnemyWaveSpawner* FindEnemyWaveSpawner(const UObject* WorldContextObject, const FName SpawnerIdToFind);

	// Every wave that spawned units; the units are already moving in their formation.
	UPROPERTY(BlueprintAssignable, Category = "Wave Spawner")
	FOnEnemyWaveSpawnerUnitsSpawned OnWaveUnitsSpawned;

	// No more waves come: the count ran out, the owning building died, or StopSpawner was called.
	UPROPERTY(BlueprintAssignable, Category = "Wave Spawner")
	FOnEnemyWaveSpawnerFinished OnSpawnerFinished;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FString GetPlacementKindName() const override;
	virtual FName GetPlacementId() const override { return SpawnerId; }
	virtual UTexture2D* GetEditorSpriteTexture() const override;
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const override;
	virtual void OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty) override;
	virtual void OnEnemyAINeverReady() override;

	// Name missions use to find this spawner (FindEnemyWaveSpawner); also shown above it in the editor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Spawner")
	FName SpawnerId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Spawner")
	EEnemyWaveSpawnerBehaviour Behaviour = EEnemyWaveSpawnerBehaviour::AttackWave;

	// Where the units appear (relative to the actor; drag the diamonds). Units are spread over the points in turn.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Spawner|Points", meta = (MakeEditWidget = "true"))
	TArray<FVector> SpawnPoints = {FVector::ZeroVector};

	// Attack wave: the attack-move path, in order. Patrol wave: the patrol points (at least two, visited at random).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Spawner|Points", meta = (MakeEditWidget = "true"))
	TArray<FVector> Waypoints = {FVector(3000.f, 0.f, 0.f)};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Spawner|Units")
	FEnemyWaveSpawnerUnits Units;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Spawner|Timing")
	FEnemyWaveSpawnerTiming Timing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Spawner|Owning Building")
	FEnemyWaveSpawnerOwnership Ownership;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Spawner|Attack",
		meta = (EditCondition = "Behaviour == EEnemyWaveSpawnerBehaviour::AttackWave", EditConditionHides))
	FEnemyWaveSpawnerAttackSettings AttackSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Spawner|Patrol",
		meta = (EditCondition = "Behaviour == EEnemyWaveSpawnerBehaviour::PatrolWave", EditConditionHides))
	FEnemyWaveSpawnerPatrolSettings PatrolSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Spawner|Minimap")
	FEnemyWaveSpawnerMinimapSettings Minimap;

private:
#if RTS_WITH_MAP_TESTS
	// -EnemyWaveSpawnerValidate configures spawners and reads their markers and unit lists.
	friend class UEnemyWaveSpawnerTestSubsystem;
	// -EnemyPlacementValidate configures a spawner as strongpoint reinforcements.
	friend class UEnemyPlacementTestSubsystem;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UTexture2D> M_AttackSprite = nullptr;

	UPROPERTY()
	TObjectPtr<UTexture2D> M_PatrolSprite = nullptr;
#endif

	UPROPERTY()
	TArray<FEnemyWaveSpawnerMinimapMarker> M_MinimapMarkers;

	// Every unit any wave of this spawner spawned; weak, the units die. Read by GetLivingSpawnedUnitCount.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_SpawnedUnits;

	// Patrol option bInfantryFleesWhenVehiclesLost: hands the wave to the enemy fallback watch.
	void WatchPatrolForVehicleLoss(const TArray<AActor*>& SpawnedUnits) const;

	// Spawned units still alive and not running to the fallback rally point; ConvergeOnAlarm sends only these.
	TArray<AActor*> GetLivingNotFleeingSpawnedUnits() const;

	// Set when the wave launches; the wave lives on this controller's wave controller.
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	FTimerHandle M_MinimapTimerHandle;

	// Wave on the enemy controller's wave controller while running; INDEX_NONE before launch and once ended.
	int32 M_ActiveWaveId = INDEX_NONE;
	int32 M_SpawnedWaveCount = 0;
	int32 M_NextMarkerIndex = 0;

	bool bM_IsRunning = false;
	bool bM_HasFinished = false;

	// ----- Launch -----
	bool GetIsValidEnemyController() const;
	bool GetIsOwningBuildingAlive() const;
	void LaunchWave(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty);
	void FinishSpawner();

	/**
	 * @brief Translates the editor settings into one wave request for the enemy controller.
	 * @param Difficulty Decides which extra unit lists are added to the base units.
	 * @param OutRequest Receives the filled request (callbacks included).
	 * @return False when the settings give no units, no waypoints or invalid owning buildings.
	 */
	bool BuildWaveRequest(const ERTSGameDifficulty Difficulty, FAttackWaveRequest& OutRequest);
	void BuildRequestTiming(FAttackWaveRequest& OutRequest) const;
	void BuildRequestMovement(const TArray<FVector>& WorldWaypoints, FAttackWaveRequest& OutRequest) const;
	void BuildRequestOwnership(FAttackWaveRequest& OutRequest) const;
	TArray<const TArray<FEnemyWaveSpawnerUnitEntry>*> GetUnitListsForDifficulty(
		const ERTSGameDifficulty Difficulty) const;

	/**
	 * @brief One wave element per unit; repeated use of a spawn point is spread on rings around it so units do not
	 * spawn inside each other.
	 * @param Difficulty Decides which extra unit lists are added.
	 * @param WorldSpawnPoints Spawn points in world space.
	 * @return The elements; empty when the lists hold no valid unit.
	 */
	TArray<FAttackWaveElement> BuildWaveElements(
		const ERTSGameDifficulty Difficulty,
		const TArray<FVector>& WorldSpawnPoints) const;

	/**
	 * @brief Adds the Count elements of one unit entry, at its own spawn point or spread over all of them in turn.
	 * @param Entry The unit entry.
	 * @param WorldSpawnPoints Spawn points in world space.
	 * @param InOutUsesPerSpawnPoint How often each point was used; picks the ring slot of the next unit there.
	 * @param InOutNextSpreadPoint Next spawn point for entries without a fixed one.
	 * @param OutElements Receives the elements.
	 */
	void AppendEntryElements(
		const FEnemyWaveSpawnerUnitEntry& Entry,
		const TArray<FVector>& WorldSpawnPoints,
		TArray<int32>& InOutUsesPerSpawnPoint,
		int32& InOutNextSpreadPoint,
		TArray<FAttackWaveElement>& OutElements) const;
	FVector GetSpreadSpawnLocation(const FVector& SpawnPoint, const int32 UseIndex) const;

	// ----- Wave callbacks -----
	void HandleWaveUnitsSpawned(const int32 WaveId, const TArray<AActor*>& SpawnedUnits);
	void HandleWaveEnded(const int32 WaveId);

	// ----- Minimap -----
	void AddMinimapMarker(const TArray<AActor*>& SpawnedUnits);
	void EnsureMinimapTimer();
	void UpdateMinimapMarkers();
	void RemoveAllMinimapMarkers();

	// ----- Editor -----
	bool GetIsPatrol() const { return Behaviour == EEnemyWaveSpawnerBehaviour::PatrolWave; }
	bool GetHasUnsetUnitEntry() const;
	FVector GetSpawnCentre(const TArray<FVector>& WorldSpawnPoints) const;
	void AddSpawnPointShapes(const TArray<FVector>& WorldSpawnPoints, FEnemyPlacementShapes& OutSummary,
	                         FEnemyPlacementShapes& OutDetail) const;
	void AddAttackPathShapes(const TArray<FVector>& WorldSpawnPoints, const TArray<FVector>& WorldWaypoints,
	                         FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const;
	void AddPatrolLoopShapes(const TArray<FVector>& WorldSpawnPoints, const TArray<FVector>& WorldWaypoints,
	                         FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const;
};
