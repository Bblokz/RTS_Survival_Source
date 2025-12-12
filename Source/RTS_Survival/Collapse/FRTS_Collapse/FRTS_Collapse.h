#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "RTS_Survival/Collapse/DestroySpawnActorsParameters.h"

#include "FRTS_Collapse.generated.h"

struct FSwapToDestroyedMesh;
class UGeometryCollection;
class UMeshComponent;

/**
 * Static utility for collapsing a mesh into a GeometryCollection, playing SFX/VFX, etc.
 */
class RTS_SURVIVAL_API FRTS_Collapse
{
public:
	/**
	 * Collapses MeshToCollapse into a GeometryCollection, applying radial impulse, optional SFX/VFX,
	 * and eventually destroying the Geo component after a delay.
	 */
	static void CollapseMesh(
		AActor* CollapseOwner,
		UGeometryCollectionComponent* GeoCollapseComp,
		TSoftObjectPtr<UGeometryCollection> GeoCollection,
		UMeshComponent* MeshToCollapse,
		FCollapseDuration CollapseDuration,
		FCollapseForce CollapseForce,
		FCollapseFX CollapseFX
	);

	/**
	 * @brief Async loads the new mesh from the provided parameters and swaps the old mesh on the actor's provided
	 * mesh component to this new mesh.
	 * @post Plays sound and visual FX if provided.
	 * @param COllapseOwner The actor that owns the component to swap on.
	 * @param SwapToDestroyedMeshParameters The parameters for the swap. 
	 */
	static void CollapseSwapMesh(
		AActor* COllapseOwner,
		FSwapToDestroyedMesh SwapToDestroyedMeshParameters);

	/**
	 * @brief Async loads all referenced actors to spawn and spawns them with their respective offsets, rotations, and scales.
	 * will destroy the owner after a delay.
	 * @param Owner The owner to destroy.
	 * @param SpawnParams Defines what actors to spawn.
	 * @param CollapseFX
	 */
	static void OnDestroySpawnActors(
		AActor* Owner,
		const FDestroySpawnActorsParameters& SpawnParams,
		FCollapseFX CollapseFX
	);

private:
	static bool GetIsValidCollapseRequest(
		const AActor* CollapseOwner,
		const UGeometryCollectionComponent* GeoCollapseComp,
		const TSoftObjectPtr<UGeometryCollection>& GeoCollection,
		const UMeshComponent* MeshToCollapse
	);
	static bool EnsureValidCollapseContext(const TSharedPtr<struct FCollapseTaskContext>& Context);

	/** Internal sequence: 1) wait next tick, 2) async load, 3) apply impulse, 4) final timer. */
	static void HandleCollapseNextTick(TSharedPtr<struct FCollapseTaskContext> Context);
	static void OnAssetLoadSucceeded(TSharedPtr<struct FCollapseTaskContext> Context);
	static void HandleDestroyGeometry(TSharedPtr<struct FCollapseTaskContext> Context);

	static void LoadSwapMeshAsset(TWeakObjectPtr<AActor> WeakOwner, FSwapToDestroyedMesh SwapParams);
	static void OnSwapMeshAssetLoaded(TWeakObjectPtr<AActor> WeakOwner, FSwapToDestroyedMesh SwapParams);


	// New private functions for spawn-on-destroy:
	static void OnSpawnActorsAssetsLoaded(TSharedPtr<struct FOnDestroySpawnActorsContext> Context);
	static void SpawnActorsFromContext(TSharedPtr<struct FOnDestroySpawnActorsContext> Context);
	static void HandleDestroyOwner(TWeakObjectPtr<AActor> Owner);

	static void SpawnCollapseVFX(const FCollapseFX& CollapseFX, UWorld* World, const FVector& BaseLocationFX);
	static void SpawnCollapseSFX(const FCollapseFX& CollapseFX, UWorld* World, const FVector& BaseLocationFX);
};

/** Holds data needed across timers / async callbacks. */
USTRUCT()
struct FCollapseTaskContext
{
	GENERATED_BODY()

	// Actor & component tracked as TWeakObjectPtr so we don't crash if destroyed
	TWeakObjectPtr<AActor> WeakOwner;
	TWeakObjectPtr<UGeometryCollectionComponent> WeakGeoComp;
	TWeakObjectPtr<UMeshComponent> WeakMeshToCollapse;

	// The geometry to load, plus references to force/fx/duration
	TSoftObjectPtr<UGeometryCollection> GeoCollection;
	UPROPERTY()
	FCollapseDuration CollapseDuration;
	UPROPERTY()
	FCollapseForce CollapseForce;
	UPROPERTY()
	FCollapseFX CollapseFX;

	FCollapseTaskContext()
	{
	}

	FCollapseTaskContext(
		AActor* Owner,
		UGeometryCollectionComponent* GeoComp,
		UMeshComponent* MeshToCollapse,
		const TSoftObjectPtr<UGeometryCollection>& CollAsset,
		const FCollapseDuration& InDuration,
		const FCollapseForce& InForce,
		const FCollapseFX& InFX
	)
		: WeakOwner(Owner)
		  , WeakGeoComp(GeoComp)
		  , WeakMeshToCollapse(MeshToCollapse)
		  , GeoCollection(CollAsset)
		  , CollapseDuration(InDuration)
		  , CollapseForce(InForce)
		  , CollapseFX(InFX)
	{
	}
};

/** Holds data needed for spawning actors upon death. */
USTRUCT()
struct FOnDestroySpawnActorsContext
{
	GENERATED_BODY()

	// The parameters for actor spawning.
	FDestroySpawnActorsParameters SpawnParams;

	// Weak pointer to the owner actor for safety.
	TWeakObjectPtr<AActor> WeakOwner;

	// Optional collapse FX that might be played.
	FCollapseFX CollapseFX;
};
