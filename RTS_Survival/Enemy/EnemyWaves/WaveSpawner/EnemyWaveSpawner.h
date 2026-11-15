// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "EnemyWaveSpawnerTypes.h"

#include "EnemyWaveSpawner.generated.h"

class AEnemyController;
class AEnemyWaveSpawner;
class UBillboardComponent;
class UTexture2D;
class UTextRenderComponent;
class UEnemyWaveSpawnerVisualComponent;
struct FEnemyWaveSpawnerVisualData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyWaveSpawnerUnitsSpawned, AEnemyWaveSpawner*, Spawner,
                                             const TArray<AActor*>&, Units);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyWaveSpawnerFinished, AEnemyWaveSpawner*, Spawner);

/**
 * @brief Placed in a mission map to author an enemy attack or patrol wave in the level instead of in mission logic.
 * Drag the SpawnPoints and Waypoints diamonds in the viewport; red = attack wave, blue = patrol wave. The wave runs
 * on the enemy controller's wave controller (wave supply, owning building, formations, direct-control handoff).
 * Nothing of it shows in game. Starts on BeginPlay, or from a mission via FindEnemyWaveSpawner + StartSpawner.
 */
UCLASS(Blueprintable, HideCategories = (Rendering, Collision, Input, HLOD, Physics, LOD, Cooking, Replication))
class RTS_SURVIVAL_API AEnemyWaveSpawner : public AActor
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
	virtual void OnConstruction(const FTransform& Transform) override;

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
#endif

	UPROPERTY()
	TObjectPtr<USceneComponent> M_Root = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> M_EditorSprite = nullptr;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> M_EditorLabel = nullptr;

	UPROPERTY()
	TObjectPtr<UEnemyWaveSpawnerVisualComponent> M_EditorVisual = nullptr;

	UPROPERTY()
	TObjectPtr<UTexture2D> M_AttackSprite = nullptr;

	UPROPERTY()
	TObjectPtr<UTexture2D> M_PatrolSprite = nullptr;
#endif

	UPROPERTY()
	TArray<FEnemyWaveSpawnerMinimapMarker> M_MinimapMarkers;

	// Set when the wave launches; the wave lives on this controller's wave controller.
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	FTimerHandle M_LaunchTimerHandle;
	FTimerHandle M_MinimapTimerHandle;

	// Wave on the enemy controller's wave controller while running; INDEX_NONE before launch and once ended.
	int32 M_ActiveWaveId = INDEX_NONE;
	int32 M_SpawnedWaveCount = 0;
	int32 M_NextMarkerIndex = 0;
	// Launch attempts made while the enemy controller or the game difficulty was not ready yet.
	int32 M_LaunchRetryCount = 0;

	bool bM_IsRunning = false;
	bool bM_HasFinished = false;

	// ----- Launch -----
	void ScheduleLaunch(const float DelaySeconds);
	void TryLaunchWave();
	bool GetIsValidEnemyController() const;
	bool TryGetGameDifficulty(ERTSGameDifficulty& OutDifficulty) const;
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
	FVector ProjectToNavigationIfPossible(const FVector& Location) const;

	TArray<FVector> GetWorldPoints(const TArray<FVector>& LocalPoints) const;

	// ----- Wave callbacks -----
	void HandleWaveUnitsSpawned(const int32 WaveId, const TArray<AActor*>& SpawnedUnits);
	void HandleWaveEnded(const int32 WaveId);

	// ----- Minimap -----
	void AddMinimapMarker(const TArray<AActor*>& SpawnedUnits);
	void EnsureMinimapTimer();
	void UpdateMinimapMarkers();
	void RemoveAllMinimapMarkers();

	// ----- Editor -----
	void RefreshEditorVisuals();
	FColor GetBehaviourColor() const;
	FString GetEditorLabelText() const;
	FString GetConfigurationProblem() const;
	void FillVisualData(FEnemyWaveSpawnerVisualData& OutVisualData) const;
};
