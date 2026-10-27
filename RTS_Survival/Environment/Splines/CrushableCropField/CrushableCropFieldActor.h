// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "RTS_Survival/Environment/DestructableEnvActor/CrushDestructionTypes/ECrushDestructionType.h"
#include "CrushableCropFieldActor.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UApartmentFootprintNavModifier;
class UMaterialInterface;
class UNavArea;
class USplineComponent;
class UStaticMesh;

/** One kind of plant in a crop field: the standing mesh and what is left of it once a vehicle drove over it. */
USTRUCT(BlueprintType)
struct FCrushableCropVariant
{
	GENERATED_BODY()

	// Standing plant, authored with its base at the origin.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> IntactMesh = nullptr;

	// Flattened remains, authored lying along +X; turned to the crushing vehicle's heading.
	// Leave empty to let the plant simply disappear.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> CrushedMesh = nullptr;

	// Relative chance of this variant among all variants of the field.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0"))
	float Weight = 1.f;
};

/** How the plants are laid out inside the outline: rows along the actor's local X axis. */
USTRUCT(BlueprintType)
struct FCrushableCropLayout
{
	GENERATED_BODY()

	// Distance between rows (local Y).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="20.0"))
	float RowSpacing = 120.f;

	// Distance between plants within a row (local X).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="20.0"))
	float PlantSpacing = 100.f;

	// Random offset of every plant as a fraction of the spacing (0 = perfect grid).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="0.5"))
	float PositionJitter = 0.2f;

	// Plants keep this distance from the outline so they do not poke through the fence.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0"))
	float EdgeInset = 60.f;

	// Chance that a plant spot stays empty, breaks up the grid.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"))
	float MissingPlantChance = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1"))
	float MinScale = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1"))
	float MaxScale = 1.15f;

	// Fully random yaw (wheat, pumpkins). If false plants follow the row (vineyards) and only get YawJitter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRandomYaw = true;

	// Rows only: half the plants are turned 180 degrees so neighbouring row sections do not repeat.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="!bRandomYaw"))
	bool bFlipHalfOfRowPlants = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="45.0"))
	float YawJitterDegrees = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RandomSeed = 1337;
};

/** Runtime bookkeeping of one plant. */
USTRUCT()
struct FCrushableCropPlant
{
	GENERATED_BODY()

	FTransform LocalTransform = FTransform::Identity;
	int32 VariantIndex = INDEX_NONE;
	int32 IntactInstanceIndex = INDEX_NONE;
	// 0..1, written to per-instance custom data 0 on both the standing and the crushed instance (tint variation).
	float TintSeed = 0.f;
	bool bCrushed = false;
};

/** A vehicle component currently inside the field whose footprint flattens plants. */
USTRUCT()
struct FCrushableCropVehicle
{
	GENERATED_BODY()

	TWeakObjectPtr<UPrimitiveComponent> Component = nullptr;
	// Footprint of the component in its own space.
	FBox LocalFootprint = FBox(ForceInit);
};

/**
 * @brief Instanced, crushable crop field (wheat, pumpkins, vineyards) whose area is a closed spline outline.
 * Works with the crushable pipes (ADestructibleSplineActor): same ECrushDestructionType rule through the shared
 * FRTSCrushGate and the same FCollapseFX contract; fences around a field are plain destructible spline actors.
 * Unlike the spline pieces, thousands of plants cannot each own a physics body, so:
 * - Plants are ISM instances without collision (GPU-scene culled per instance, cheap to update while crushing),
 *   rows along local X, one component per mesh (one draw per mesh).
 * - Every VehicleScanInterval one overlap query over the field finds vehicles (no collision primitive of its own, so
 *   cover, guard-surface and AOE object queries never see a field); while a vehicle that passes the crush rule is
 *   inside, its oriented footprint is sampled on a short timer against a grid of plants.
 * - A crushed plant swaps to the variant's flattened mesh, turned to the vehicle's heading, so tanks leave trails.
 * - Optionally standing plants mark a nav area (e.g. vineyards block light tanks) that opens up where crushed.
 * @note Rebuilt on construction (editor preview) and on BeginPlay from a fixed seed, so both match.
 */
UCLASS()
class RTS_SURVIVAL_API ACrushableCropFieldActor : public AActor
{
	GENERATED_BODY()

public:
	ACrushableCropFieldActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	// Routes radial damage (explosions) to the plants: everything inside the inner part of the blast is flattened.
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

	/** Closed outline of the field in the actor's XY plane; rows run along local X. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Field")
	TObjectPtr<USplineComponent> FieldOutline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Field")
	TArray<FCrushableCropVariant> Variants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Field")
	FCrushableCropLayout Layout;

	// Optional material for every plant mesh (the farm atlas material is already on the meshes).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Field")
	TObjectPtr<UMaterialInterface> MaterialOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Field|Rendering")
	bool bStandingPlantsCastShadow = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Field|Rendering")
	bool bCrushedPlantsCastShadow = false;

	/** Which vehicles flatten the plants (same rule as the crushable spline pieces). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crush")
	ECrushDestructionType CrushDestructionType = ECrushDestructionType::AnyTank;

	// Grows every vehicle footprint so plants brushing the hull sides go down too.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crush", meta=(ClampMin="0.0"))
	float CrushFootprintPadding = 25.f;

	// Seconds between footprint samples while a crushing vehicle is inside the field.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crush", meta=(ClampMin="0.02"))
	float CrushSampleInterval = 0.1f;

	// Seconds between the overlap queries that look for vehicles entering the field.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crush", meta=(ClampMin="0.05"))
	float VehicleScanInterval = 0.25f;

	// Explosions flatten plants within this fraction of the blast's outer radius (0 disables).
	// Only reached by damage applied to this actor directly: the field has no collision for AOE sweeps to hit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crush", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ExplosionCrushRadiusFraction = 0.5f;

	/** Played at crushed plants; the sound plays at most once per CrushSoundCooldown for the whole field. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crush")
	FCollapseFX CrushFX;

	// At most this many VFX per vehicle per sample (spread over the crushed plants).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crush", meta=(ClampMin="0"))
	int32 MaxCrushVfxPerSample = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crush", meta=(ClampMin="0.0"))
	float CrushSoundCooldown = 0.45f;

	/**
	 * If set, every standing plant marks this nav area over its own cell (spacing x row spacing) and the cell
	 * opens up once the plant is crushed. Use an area that only the blocked agents' query filters exclude.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
	TSubclassOf<UNavArea> StandingPlantsNavArea = nullptr;

	UFUNCTION(CallInEditor, Category="Field")
	void RebuildField();

	/**
	 * @brief Flattens every standing plant within Radius of WorldLocation (XY distance).
	 * @return How many plants were crushed.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Crush")
	int32 CrushPlantsInRadius(const FVector WorldLocation, const float Radius);

	UFUNCTION(BlueprintPure, Category="Field")
	int32 GetNumStandingPlants() const { return M_NumStandingPlants; }

	UFUNCTION(BlueprintPure, Category="Field")
	int32 GetNumPlants() const { return M_Plants.Num(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Called after a footprint sample or explosion flattened plants.
	UFUNCTION(BlueprintImplementableEvent, Category="Crush")
	void BP_OnPlantsCrushed(int32 NumCrushed);

private:
	// Only the volume the vehicle scan queries: no collision, so it never shows up in other systems' object queries.
	UPROPERTY(VisibleAnywhere, Category="Crush")
	TObjectPtr<UBoxComponent> M_VehicleDetectionBox;

	UPROPERTY(VisibleAnywhere, Category="Navigation")
	TObjectPtr<UApartmentFootprintNavModifier> M_StandingPlantsNav;

	// One per variant; index == variant index. Created on build, never saved.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> M_IntactComponents;

	// One per variant (null where the variant has no crushed mesh).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> M_CrushedComponents;

	UPROPERTY(Transient)
	TArray<FCrushableCropPlant> M_Plants;

	UPROPERTY(Transient)
	TArray<FCrushableCropVehicle> M_CrushingVehicles;

	bool GetIsValidVehicleDetectionBox() const;
	bool GetIsValidStandingPlantsNav() const;

	// ------------------------- BUILD ----------------------------------

	void ClearField();
	void BuildField();

	/** @return Sum of the weights of every usable variant (0 = nothing to place). */
	float GetTotalVariantWeight() const;

	/** One standing component per variant and one crushed component per distinct crushed mesh. */
	void CreateVariantComponents();

	/** Fills M_Plants with the rows inside the outline (deterministic from Layout.RandomSeed). */
	void PlacePlants(const TArray<FVector2D>& Polygon, const float TotalWeight);

	/**
	 * @brief Rolls one plant at a grid spot; may skip it (missing plant, jitter out of the outline).
	 * Always consumes the same random numbers so one skipped spot never shifts the rest of the layout.
	 */
	void TryAddPlant(const FRandomStream& Stream, const float TotalWeight, const TArray<FVector2D>& Polygon,
	                 const FVector2D& GridSpot);

	/** Adds every plant as an instance of its variant's component and writes its tint seed. */
	void AddPlantInstances();

	/** @return The outline as a closed polygon in local XY, empty when the spline has fewer than 3 points. */
	TArray<FVector2D> GetLocalOutlinePolygon() const;

	/** Inside intervals [MinX, MaxX] of the polygon along the horizontal line LocalY, shrunk by Inset. */
	static void GetRowIntervals(const TArray<FVector2D>& Polygon, const float LocalY, const float Inset,
	                            TArray<FVector2D>& OutIntervals);

	static bool GetIsInsidePolygon(const TArray<FVector2D>& Polygon, const FVector2D& Point);

	/** @return Distance from Point to the nearest polygon edge. */
	static float GetDistanceToOutline(const TArray<FVector2D>& Polygon, const FVector2D& Point);

	/** Picks a variant by weight; INDEX_NONE when no variant is usable. */
	int32 PickVariant(const FRandomStream& Stream, const float TotalWeight) const;

	/** Local ground height under a local XY point (first world-static hit that is not a volume), or 0 when nothing is hit. */
	float TraceLocalGroundZ(const FVector2D& LocalPoint) const;

	UInstancedStaticMeshComponent* CreatePlantComponent(UStaticMesh* Mesh, const bool bCastShadow,
	                                                                 const TCHAR* NamePrefix, const int32 Index);

	void BuildPlantGrid();
	void UpdateDetectionBox();

	// ------------------------- CRUSH ----------------------------------

	/** Starts the periodic vehicle scan (game world only, while plants stand). */
	void BeginPlay_StartVehicleScan();

	/** Overlap query over the field; tracks every vehicle passing the crush rule, by its root primitive. */
	void ScanForVehicles();

	/** @return The tracked entry for Vehicle's root primitive, reusing its footprint when it was tracked already. */
	FCrushableCropVehicle MakeTrackedVehicle(UPrimitiveComponent* VehicleRoot) const;

	void SampleCrushingVehicles();

	/**
	 * @brief Crushes the standing plants inside the component's padded, oriented footprint.
	 * @param OutCrushedWorldLocations Receives the world location of every plant crushed by this vehicle.
	 */
	void CrushUnderVehicle(const FCrushableCropVehicle& Vehicle, TArray<FVector>& OutCrushedWorldLocations);

	/** Swaps one plant to its crushed mesh lying along LocalHeadingYaw; leaves render/nav refresh to the caller. */
	void CrushPlant(const int32 PlantIndex, const float LocalHeadingYaw);

	/** Pushes the instance changes to the renderer and schedules the nav refresh after a crush batch. */
	void FinishCrushBatch(const int32 NumCrushed);

	/** VFX at up to MaxCrushVfxPerSample of the locations, the sound once (respecting CrushSoundCooldown). */
	void PlayCrushFX(const TArray<FVector>& CrushedWorldLocations);

	void UpdateStandingPlantsNav();

	/** Schedules one UpdateStandingPlantsNav for all crushes within NavRefreshDelay. */
	void RequestStandingPlantsNavRefresh();

	/** Plant indices whose grid cells overlap the local XY box. */
	void GatherPlantsInLocalBox(const FBox2D& LocalBox, TArray<int32>& OutPlantIndices) const;

	// ------------------------- STATE ----------------------------------

	// Plant grid in local XY; every cell lists the plants whose origin falls inside it.
	FVector2D M_GridOrigin = FVector2D::ZeroVector;
	float M_GridCellSize = 200.f;
	FIntPoint M_GridDimensions = FIntPoint::ZeroValue;
	TArray<TArray<int32>> M_GridCells;

	FTimerHandle M_VehicleScanTimer;
	FTimerHandle M_CrushSampleTimer;
	// Nav refreshes rebuild every tile under the field for every agent, so crush batches are merged.
	FTimerHandle M_NavRefreshTimer;
	int32 M_NumStandingPlants = 0;
	double M_LastCrushSoundTime = -1000.0;
	bool bM_InstancesDirty = false;
	bool bM_NavDirty = false;
};
