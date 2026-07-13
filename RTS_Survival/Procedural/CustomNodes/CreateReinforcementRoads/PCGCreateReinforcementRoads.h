// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGContext.h"
#include "PCGSettings.h"

#include "PCGCreateReinforcementRoads.generated.h"

class UMaterialInterface;
class UStaticMesh;
class AActor;

/**
 * @brief Describes one Blueprint pole option used while populating a reinforcement road.
 * The orientation axis lets differently-authored pole assets align consistently with the wire route.
 */
USTRUCT(BlueprintType)
struct FReinforcementRoadPoleEntry
{
	GENERATED_BODY()

	/** Blueprint actor containing an initialized UDestructibleWire component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Electric Poles")
	TSoftClassPtr<AActor> PoleActor;

	/** Preferred wire count; each span uses the smaller preference of its two poles. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Electric Poles", meta = (ClampMin = "1"))
	int32 PreferredAmountWires = 1;

	/** Local horizontal axis aligned with the road direction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Electric Poles")
	FVector Orientation = FVector::ForwardVector;

	/** Distance from the road centerline; one side is selected for the entire pole chain. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Electric Poles", meta = (ClampMin = "0"))
	float OffsetFromRoad = 500.0f;
};

/**
 * @brief Controls the assets and variable spacing for one reinforcement-road pole category.
 * A category is eligible only when it contains at least one configured pole actor.
 */
USTRUCT(BlueprintType)
struct FReinforcementRoadPoleCategory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Electric Poles")
	TArray<FReinforcementRoadPoleEntry> Poles;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Electric Poles", meta = (ClampMin = "100"))
	float MinimumDistanceBetweenPoles = 1000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Electric Poles", meta = (ClampMin = "100"))
	float MaximumDistanceBetweenPoles = 1400.0f;
};

/**
 * @brief Configures managed reinforcement-road splines generated between compatible endpoints.
 * The node pairs each endpoint at most once and falls back to backup ends when no natural primary route exists.
 * @note Generated spline actors and their road meshes are owned by the executing PCG component.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGCreateReinforcementRoadsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Strength of the decorative, snake-like displacement. Required connection turns may be sharper. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Road", meta = (ClampMin = "0", PCG_Overridable))
	float Curvature = 0.5f;

	/** Approximate number of decorative bends added per 1000 Unreal units of road. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Road", meta = (ClampMin = "0", PCG_Overridable))
	float AmountCurvesPer1000Units = 0.35f;

	/** Mesh deformed along every generated spline. Its local X extent determines mesh segment length. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Road")
	TSoftObjectPtr<UStaticMesh> RoadMesh;

	/** Optional material applied to slot zero of every generated spline mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Road")
	TSoftObjectPtr<UMaterialInterface> OverrideMaterial;

	/** Hard rise/run grade limit used while searching; 0.08 means an eight-percent grade. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Road|Terrain Avoidance",
		meta = (ClampMin = "0.01", ClampMax = "0.5", PCG_Overridable))
	float MaximumTerrainGrade = 0.08f;

	/** Terrain this far above both endpoints is treated as an impassable hill. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Road|Terrain Avoidance",
		meta = (ClampMin = "0", PCG_Overridable))
	float MaximumElevationAboveEndpoints = 250.0f;

	/** Cost multiplier that makes flatter detours preferable before the hard limits are reached. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Road|Terrain Avoidance",
		meta = (ClampMin = "0", PCG_Overridable))
	float HillAvoidanceStrength = 250.0f;

	/** Large pole options and their spacing range. One eligible category is selected per road. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Electric Poles")
	FReinforcementRoadPoleCategory LargeElectricPoles;

	/** Small pole options and their spacing range. One eligible category is selected per road. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Electric Poles")
	FReinforcementRoadPoleCategory SmallElectricPoles;

	/** Mesh used by every wire span created between adjacent poles. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Electric Poles")
	TSoftObjectPtr<UStaticMesh> WireMesh;

	/** Scale applied to the shared wire mesh along every generated span. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Electric Poles")
	FVector WireMeshScale = FVector::OneVector;
};

/**
 * @brief Selects one-use road endpoints, plans exclusion-safe natural routes, follows terrain,
 * spawns managed spline meshes, and emits sampled paths plus mesh-bound volumes for downstream PCG logic.
 */
class FPCGCreateReinforcementRoadsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
