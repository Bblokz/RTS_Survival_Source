// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PCGContext.h"
#include "PCGSettings.h"

#include "SurvivalSovietBasePCGNodes.generated.h"

class UBoxComponent;
class UPCGComponent;
class UPCGGraphInterface;
class USurvivalSovietBaseComponent;
struct FSurvivalSovietBaseState;

/** @brief One step of the base generation graph. */
UENUM(BlueprintType)
enum class ESurvivalSovietPlanStage : uint8
{
	// Picks gate and axis from the terrain, draws the archetype's master plan, buys down the threat path.
	MasterPlan,
	// Streets and gates. Comes first of what stands on the ground: it is what keeps the base drivable.
	Streets,
	Buildings,
	VehiclePark,
	Guns,
	Perimeter,
	Dressing,
	// Spends what lost spots gave back and assigns every unit a post.
	Garrison
};

/**
 * @brief The actor a base's PCG component runs on: one per base, placed on the start location by
 * USurvivalSovietBaseComponent, which also hands it the graph. It tells the graph's nodes which base they build.
 * @note Spawned at runtime only; never placed in a level.
 */
UCLASS(NotPlaceable, Transient)
class RTS_SURVIVAL_API ASurvivalSovietBasePCGVolume : public AActor
{
	GENERATED_BODY()

public:
	ASurvivalSovietBasePCGVolume();

	/**
	 * @brief Starts the graph for this base.
	 * @param InBaseArrayIndex The base inside the component's array.
	 * @param Radius Half size of the volume: the area the graph works on.
	 */
	void StartGeneration(USurvivalSovietBaseComponent* SovietBases, int32 InBaseArrayIndex, UPCGGraphInterface* Graph,
	                     int32 Seed, float Radius);

	USurvivalSovietBaseComponent* GetSovietBases() const { return M_SovietBases.Get(); }
	int32 GetBaseArrayIndex() const { return M_BaseArrayIndex; }

private:
	UPROPERTY()
	TObjectPtr<UBoxComponent> M_Bounds = nullptr;

	UPROPERTY()
	TObjectPtr<UPCGComponent> M_PCGComponent = nullptr;

	TWeakObjectPtr<USurvivalSovietBaseComponent> M_SovietBases;
	int32 M_BaseArrayIndex = INDEX_NONE;
};

/**
 * @brief First graph node: measures this base's Blueprints and traces its terrain, spread over frames. Outputs the
 * terrain as points (Density 0 = no ground, Steepness = local slope), so later graph nodes can read it.
 * @note Does nothing outside a graph run by ASurvivalSovietBasePCGVolume.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGSovietBaseSiteSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SovietBaseSite")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

/**
 * @brief One stage of a Soviet base layout. Stages run in graph order on one shared occupancy, so they are chained
 * through their After pins. Each outputs what it placed as points (attributes ClassIndex, Role), which is what the
 * spawn node builds; filter or drop a Placed output in the graph and that content is not built.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGSovietBaseStageSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SovietBaseStage")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
	virtual FString GetAdditionalTitleInformation() const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Stage")
	ESurvivalSovietPlanStage Stage = ESurvivalSovietPlanStage::MasterPlan;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

/**
 * @brief Last graph node: spawns what the stages' points describe in the component's batches over frames and finishes
 * when the base stands. Roads arrive as point chains (attributes Road, RoadKind, RoadWidth).
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGSovietBaseSpawnSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SovietBaseSpawn")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

/** @brief Runs the site node; asleep between the component's preparation steps. */
class FPCGSovietBaseSiteElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};

/** @brief Runs one layout stage. */
class FPCGSovietBaseStageElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};

/** @brief Runs the spawn node; asleep between the component's spawn batches. */
class FPCGSovietBaseSpawnElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
