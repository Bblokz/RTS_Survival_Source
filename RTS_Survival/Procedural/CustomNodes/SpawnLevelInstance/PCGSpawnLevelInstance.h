// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGContext.h"
#include "PCGSpawnLevelInstance.generated.h"

class ALevelInstance;

/**
 * @brief Bookkeeping for one spawned level instance while the element resolves its bounds,
 * which can take multiple frames at runtime because level instances stream in asynchronously.
 */
struct FPCGSpawnedLevelInstanceEntry
{
	// Weak because the actor is owned by the world; PCG managed resources own its lifetime.
	TWeakObjectPtr<ALevelInstance> Actor;

	// Full spawn transform (pivot location + ZOffset, chosen yaw); orients the output bounds point.
	FTransform SpawnTransform = FTransform::Identity;

	// Seed derived from the node seed and the pivot point's seed; forwarded to the bounds point.
	int32 PivotSeed = 0;

	// Bounds in the local space of the spawned level instance (relative to its pivot).
	FBox LocalBounds = FBox(EForceInit::ForceInit);

	bool bBoundsResolved = false;
};

/**
 * @brief Custom PCG context so the element can spawn once and then keep polling
 * across executions until all runtime-streamed level instances report their bounds.
 */
struct FPCGSpawnLevelInstanceContext : public FPCGContext
{
	TArray<FPCGSpawnedLevelInstanceEntry> SpawnedEntries;

	// True once actors have been spawned; subsequent executions only resolve bounds.
	bool bSpawnPhaseComplete = false;

	// Wall-clock time (FPlatformTime::Seconds) when waiting on streaming began; -1 until the wait starts.
	// Time-based so the timeout is independent of how often the executor re-invokes the element.
	double LoadWaitStartTimeSeconds = -1.0;
};

/**
 * @brief Settings for the SpawnLevelInstance node. Treats each input point as a pivot,
 * picks one of the configured level instance assets and a yaw turn using the seed,
 * and spawns the level instance there. Outputs the pivots and the oriented bounds
 * of what was spawned so downstream nodes can exclude overlapping points.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGSpawnLevelInstanceSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

	/** @brief This node picks the level asset and yaw turn from the seed. */
	virtual bool UseSeed() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	/**
	 * @brief Level assets to choose from; one is picked per pivot point using the seed.
	 * Null entries are ignored.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<TSoftObjectPtr<UWorld>> LevelInstances;

	/** @brief If true, the picked yaw may be 0, 90, 180 or 270 degrees. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAllow90DegTurns = false;

	/** @brief If true, additionally allows the diagonal turns (45, 135, 225, 315 degrees). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAllow45DegTurns = false;

	/** @brief Z offset applied to each spawned level instance, relative to its pivot point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float ZOffset = 0.0f;
};

/**
 * @brief Execution element for SpawnLevelInstance. Spawns the level instance actors on the
 * main thread, registers them as PCG managed actors for proper cleanup on regenerate,
 * and emits the pivots plus one bounds point per spawned instance (bounds follow the chosen yaw).
 */
class FPCGSpawnLevelInstanceElement : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	/**
	 * @brief Spawns on the first execution, then returns false (still running) until the
	 * bounds of all spawned level instances are known; needed because runtime streaming is async.
	 * @param Context The PCG context; must be an FPCGSpawnLevelInstanceContext.
	 * @return True once all outputs (Pivots and Bounds) have been produced.
	 */
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	/** @brief Actor spawning must happen on the game thread. */
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

	/** @brief Spawning actors is a side effect; results must never come from the cache. */
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
