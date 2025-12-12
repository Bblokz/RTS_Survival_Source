// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "GridOverlaySettings.h"
#include "GridOverlayTileType.h"
#include "GameFramework/Actor.h"
#include "BuildingGridOverlay.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * @brief Draws a flat, instanced grid overlay used during building placement.
 *        World-aligned to a virtual grid so preview sessions are consistent.
 * @note RebuildGrid: call in blueprint after changing settings at runtime to rebuild the grid.
 */
UCLASS()
class RTS_SURVIVAL_API ABuildingGridOverlay : public AActor
{
	GENERATED_BODY()

public:
	ABuildingGridOverlay();

	/**
	 * @brief Rebuild the instanced grid using the current settings.
	 *        Uses per-instance scale so each tile matches CellSize exactly.
	 */
	UFUNCTION(BlueprintCallable, Category="GridOverlay")
	void RebuildGrid();

	/** Recompute overlap state for all instances (Vacant/Invalid + Valid for footprint). */
	UFUNCTION(BlueprintCallable, Category="GridOverlay|Collision")
	void UpdateOverlaps_ForAllTiles();

	/** Recompute overlap state for a given set of instances (Vacant/Invalid + Valid for footprint). */
	UFUNCTION(BlueprintCallable, Category="GridOverlay|Collision")
	void UpdateOverlaps_ForTiles(const TArray<int32>& InstanceIndices);

	/** External ignore list (e.g., the preview actor) for overlap tests. */
	void SetExtraOverlapActorsToIgnore(const TArray<AActor*>& ActorsToIgnore);

	// ---------- Construction preview footprint ----------
	/** Set which instances are the construction preview footprint (pre-colored Valid when free). */
	void SetConstructionPreviewSquares(const TArray<int32>& InstanceIndices);

	/** @return true if all construction preview squares are NOT overlapped (i.e., all valid/green). */
	bool AreConstructionPreviewSquaresValid() const;

	// ---------- Public read-only grid info for helpers ----------
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="GridOverlay|GridInfo")
	float GetCellSize() const { return M_GridOverlaySettings.CellSize; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="GridOverlay|GridInfo")
	int32 GetGridCount() const { return M_GridOverlaySettings.MaxSize; }

	bool GetInstanceWorldTransform(int32 InstanceIndex, FTransform& OutWorld) const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Designer-facing settings for the grid (tile size, colors, mesh, etc.). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="GridOverlay")
	FGridOverlaySettings M_GridOverlaySettings;

	/** Small Z lift to avoid z-fighting with ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GridOverlay", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_ZOffset = 10.0f;

	/** Half-height used for per-tile overlap query (world units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GridOverlay|Collision", meta=(ClampMin="1.0", UIMin="1.0"))
	float M_PerTileOverlapHalfHeight = 100.0f;

	/** XY shrink (units) applied to the per-tile box for robust overlap tests (avoid neighbor touching).
	 * in cm.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GridOverlay|Collision", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_PerTileShrink = 0.5f;

private:
	/** Instanced grid of tiles; one draw call for all cells. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInstancedStaticMeshComponent> M_GridISM = nullptr;

	/** Dynamic material instance applied to M_GridISM. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> M_GridMID = nullptr;

	// World-aligned grid anchor (minimum cell center indices covered by this overlay build).
	int32 M_MinCellX = 0;
	int32 M_MinCellY = 0;

	// ---------- Footprint state ----------
	// The instance indices forming the construction preview footprint.
	UPROPERTY()
	TArray<int32> M_ConstructPreviewIndices;

	// Fast membership test for footprint.
	UPROPERTY()
	TSet<int32> M_ConstructPreviewIndexSet;

	// Indices currently overlapped (blocked).
	UPROPERTY()
	TSet<int32> M_OverlappedIndexSet;
	
	ECollisionChannel GetPlacementQueryChannel() const;

	// ---------- Core safety / validation ----------
	bool GetIsValidGridISM() const;
	bool ValidateSettings() const;
	bool GetMeshXYSize(const UStaticMesh* InMesh, float& OutSizeX, float& OutSizeY) const;

	// ---------- Build / material ----------
	void ClearInstances() const;
	void BuildInstances();
	void TryBuildFromConstructor();
	bool EnsureDynamicMaterial();
	void UpdateMaterialParametersWithSettings() const;
	void UpdateFallOffFootprintParameters() const;

	// ---------- Collision / overlap logic ----------
	void EnsureCollisionSetup();
	static FCollisionObjectQueryParams BuildObjectQueryForPlacement();
	void UpdateSingleInstanceOverlapState(int32 InstanceIndex);
	static float TileTypeToCustomData(EGridOverlayTileType TileType);
	void FlushCustomDataRenderState() const;

	// ---------- Debug ----------
	void Debug_DrawConstructionPreviewSquares(float TimeSeconds) const;

	// Extra actors to ignore during overlap tests (e.g. owning preview actor).
	UPROPERTY()
	TArray<AActor*> M_ExtraOverlapActorsToIgnore = {};


	/**
     * @brief Compute Manhattan ring distance (in tiles) from the current footprint and
     *        write it to Per-Instance Custom Data index 1.
     *        Footprint tiles get 0, their 4-neighbors get 1, etc.
     */
    void ComputeAndWriteRings_FromFootprint();
};
