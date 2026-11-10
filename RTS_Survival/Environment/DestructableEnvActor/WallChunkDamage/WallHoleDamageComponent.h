// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WallDamageShared.h"

#include "WallHoleDamageComponent.generated.h"

class UHealthComponent;
class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;
class UStaticMeshComponent;
struct FDamageEvent;

namespace WallHoleMask
{
	// Custom primitive data layout the procedural hole materials read (Tools/WallChunks/make_wall_holes_material.py).
	// 8..10: grid cells, one bit per cell, 20 per float. 11..34: 6 holes as (mesh X, mesh Y, mesh Z, radius).
	inline constexpr int32 FirstGridPrimitiveDataIndex = 8;
	inline constexpr int32 CellsPerGridFloat = 20;
	inline constexpr int32 MaxGridFloats = 3;
	inline constexpr int32 MaxGridCells = CellsPerGridFloat * MaxGridFloats;
	inline constexpr int32 FirstHolePrimitiveDataIndex = 11;
	inline constexpr int32 FloatsPerHole = 4;
	inline constexpr int32 MaxHoles = 6;
}

// Which mesh axis the wall runs along; the grid of knock-out cells lies on that axis and Z. Must match the material
// instance's WallGridAlongY (0 for AlongX, 1 for AlongY) and WallGridColumns (0 for NoGrid).
UENUM(BlueprintType)
enum class EWallHoleGridAxis : uint8
{
	AlongX,
	AlongY,
	// Towers and other shapes that are not one flat wall: holes only; full slots merge the two nearest holes.
	NoGrid
};

USTRUCT(BlueprintType)
struct FWallHoleGridSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	EWallHoleGridAxis GridAxis = EWallHoleGridAxis::AlongX;

	// Must match the material instance's WallGridCellSize; columns and rows come from the mesh bounds the same way
	// (round(size / cell), the last column and row take the remainder).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	float CellSize = 150.f;

	// A hole at least this share of a cell wide knocks out the whole cell it hit (structural failure).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	float CellBreakRadiusShare = 0.75f;

	// When all hole slots are used, the oldest hole turns into the cells it covers; cells within its radius plus this
	// share of a cell go.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	float BakeReachShare = 0.35f;

	// A hit inside an existing hole grows it instead of taking a slot: new radius = distance + this x hit radius.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	float GrowRadiusShare = 0.7f;

	// Holes smaller than this are not worth a slot.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	float MinHoleRadius = 12.f;

	// Moves the hole sphere back toward the shooter by this share of its radius, so it bites a shallow scoop instead of
	// a ball centred on the surface. Round, thin shapes (tree trunks) use about 0.5 so a hole doesn't cut them through;
	// walls keep 0. The bite is radius x (1 - share) deep and 2 x radius x sqrt(1 - share²) wide.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles", meta=(ClampMin="0.0", ClampMax="0.9"))
	float HoleCentreOutsetShare = 0.f;

	// Face area each flying piece stands for (cm²).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	float AreaPerDebrisPiece = 4500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	int32 MaxPiecesPerHit = 12;

	// Spacing of the pieces thrown when the wall collapses.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	float CollapsePieceSpacing = 110.f;

	// Most pieces one collapse throws (big towers).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	int32 MaxCollapsePieces = 120;
};

// Where a hit's debris starts on a closed shape (towers, trees): the struck surface and the way out of it, mesh space.
// Pieces sampled inside the hole sphere would start inside a hollow tower and fly into it, out of sight.
struct FWallHolePieceAnchor
{
	FVector SurfacePoint = FVector::ZeroVector;
	FVector Outward = FVector::ZeroVector;

	bool GetIsSet() const { return not Outward.IsNearlyZero(); }
};

USTRUCT()
struct FWallHole
{
	GENERATED_BODY()

	// Sphere centre in the wall mesh's space (on the struck surface).
	UPROPERTY()
	FVector Centre = FVector::ZeroVector;

	UPROPERTY()
	float Radius = 0.f;
};

/**
 * @brief Procedural wall holes without extra geometry (WallHitMasking.md options A + B): a hit writes a noise-edged
 * sphere hole into custom primitive data, big hits and overflowing holes knock out whole grid cells, and the missing
 * part flies off as pooled Niagara debris. ADestructableEnvActor and AHPActorObjectsMaster (building expansions)
 * forward hits; every heal of the owner's UHealthComponent closes the newest damage first.
 * @note Set WallMeshComponentName, DamagedMaterials (masked _WallHoles instances per slot), DebrisMeshes and
 * GridSettings (axis and cell size matching the material instance).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UWallHoleDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWallHoleDamageComponent();

	/**
	 * @brief Called by the owner after it applied the damage; cuts a hole at the hit.
	 * @param DamageAmount Damage after the owner's reduction.
	 * @param DamageEvent A point event carries the impact; any other event uses the causer's position (splash).
	 * @param DamageCauser The projectile, bomb or ability actor; may be null.
	 * @param HealthBefore Owner health before this hit.
	 * @param HealthAfter Owner health after this hit.
	 */
	void OnOwnerDamaged(float DamageAmount, const FDamageEvent& DamageEvent, const AActor* DamageCauser,
	                    float HealthBefore, float HealthAfter);

	/**
	 * @brief Repairs bring the wall back: the newest holes and cells close until the damage fits the health again.
	 * @param HealthAfter Owner health after the repair.
	 * @param MaxHealth Owner max health.
	 */
	void OnOwnerHealed(float HealthAfter, float MaxHealth);

	// Owners with a known max health (building expansions) set it on every hit and repair.
	void SetOwnerMaxHealth(const float MaxHealth)
	{
		M_MaxOwnerHealth = MaxHealth;
		bM_HasKnownMaxHealth = true;
	}

	/**
	 * @brief Throws everything still standing as debris. Runs once.
	 * @param bHideAndDestroyOwner Destructible walls vanish and die after the debris; building expansions keep their
	 * mesh for their own death (vertical sink).
	 */
	UFUNCTION(BlueprintCallable, Category="WallHoles")
	void CollapseIntoPieces(bool bHideAndDestroyOwner = true);

	UFUNCTION(BlueprintPure, Category="WallHoles")
	int32 GetHoleCount() const { return M_Holes.Num(); }

	UFUNCTION(BlueprintPure, Category="WallHoles")
	int32 GetHiddenCellCount() const;

	/**
	 * @brief Test hit without a projectile, for tuning.
	 * @param WorldLocation Impact point on the wall.
	 * @param ShotDirection Direction the shell travelled.
	 * @param CalibreMm Calibre the hole is sized for.
	 * @return Face area this hit removed, in m².
	 */
	UFUNCTION(BlueprintCallable, Category="WallHoles|Debug")
	float Debug_FireTestHit(FVector WorldLocation, FVector ShotDirection, float CalibreMm);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	FName WallMeshComponentName = TEXT("StaticMesh");

	// Masked materials per slot, swapped in on the first hole; the intact wall keeps its opaque ones.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	TArray<TObjectPtr<UMaterialInterface>> DamagedMaterials;

	// Pieces the missing parts fly off as, scaled to the area each stands for.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	TArray<TObjectPtr<UStaticMesh>> DebrisMeshes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	FWallChunkHoleSettings HoleSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	FWallHoleGridSettings GridSettings;

	// bCollapseIntoChunksOnDeath: destructible walls hide and die; building expansions throw the debris and keep
	// their own death.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	FWallChunkCollapseSettings CollapseSettings;

private:
	void BeginPlay_FindWallMesh();
	void CacheWallShape();
	// Building expansions swap their mesh when construction finishes; the grid and bounds follow the current mesh.
	void EnsureWallShapeIsCurrent();
	void BeginPlay_CacheDebrisAreas();
	void BeginPlay_BindOwnerDeath();
	// Owners with a health component (building expansions, garrisons, health walls) close holes on every heal.
	void BeginPlay_BindOwnerHealing();
	void OnOwnerHealthHealed(float HealthAfter, float MaxHealth);

	/** @return Where the hole sphere goes: on the surface, or moved back along the shot by HoleCentreOutsetShare. */
	FVector GetOutsetHoleCentre(const FVector& SurfacePoint, const FTransform& MeshTransform, const FVector& ShotDirection,
	                            float Radius) const;

	/**
	 * @brief Closed shapes (no grid) throw debris from the struck surface outward; walls keep their shell spread and
	 * throw through the face along the shot.
	 * @param SurfacePoint Struck point in mesh space.
	 * @param MeshTransform The wall mesh's transform.
	 * @param ShotDirection World shot direction.
	 * @return An unset anchor for walls with a grid.
	 */
	FWallHolePieceAnchor MakePieceAnchor(const FVector& SurfacePoint, const FTransform& MeshTransform,
	                                     const FVector& ShotDirection) const;

	/**
	 * @brief Grows the hole the hit lands in, or takes a slot (baking or merging the oldest hole when full), and
	 * knocks out the hit cell for a big enough hole.
	 * @param MeshPoint Hole centre in mesh space.
	 * @param Radius Hole radius after the health cap.
	 * @param Anchor Where closed shapes start their pieces; unset for walls.
	 * @param OutPieces Receives the debris for the removed part; a hit on a hole at its size cap still chips the rim.
	 * @return Face area removed (cm²), an estimate that ignores overlaps.
	 */
	float CutHole(const FVector& MeshPoint, float Radius, const FWallHolePieceAnchor& Anchor,
	              TArray<FWallDebrisPiece>& OutPieces);
	float FreeOldestSlot(TArray<FWallDebrisPiece>& OutPieces);
	float BakeOldestHoleIntoCells(TArray<FWallDebrisPiece>& OutPieces);
	void MergeOldestHoleIntoNearest();
	float HideCell(int32 CellIndex, TArray<FWallDebrisPiece>& OutPieces);

	/**
	 * @brief Debris for a removed sphere shell on the wall, one piece per AreaPerDebrisPiece.
	 * @param Centre Hole centre in mesh space.
	 * @param InnerRadius Zero for a new hole, the old radius for a grown one.
	 * @param OuterRadius Outer radius.
	 * @param Anchor When set, pieces start on the struck surface around it instead of inside the sphere shell.
	 * @param OutPieces Receives the pieces.
	 */
	void AddHolePieces(const FVector& Centre, float InnerRadius, float OuterRadius, const FWallHolePieceAnchor& Anchor,
	                   TArray<FWallDebrisPiece>& OutPieces) const;
	/** @return One piece's start in mesh space: around the anchor when set, else inside the removed shell. */
	FVector GetHolePieceStart(const FVector& Centre, float InnerRadius, float OuterRadius,
	                          const FWallHolePieceAnchor& Anchor) const;
	void AddPiece(const FVector& MeshPoint, float PieceArea, TArray<FWallDebrisPiece>& OutPieces) const;

	bool GetHasGrid() const { return GridSettings.GridAxis != EWallHoleGridAxis::NoGrid; }
	// Position along the wall (mesh X or Y) and height (mesh Z): the grid's plane.
	FVector2D GetGridPoint(const FVector& MeshPoint) const;
	FVector GetMeshPointFromGrid(const FVector2D& GridPoint) const;
	int32 GetCellIndexAt(const FVector& MeshPoint) const;
	void GetCellRect(int32 CellIndex, FVector2D& OutMin, FVector2D& OutMax) const;
	bool GetIsCellHidden(int32 CellIndex) const;
	bool GetIsMeshPointRemoved(const FVector& MeshPoint) const;
	float GetRemovedAreaEstimate() const;
	float GetFaceArea() const;
	float GetAllowedRemovedArea(float HealthFractionAfter) const;

	float ApplyHit(const FVector& WorldHit, const FVector& ShotDirection, float HoleRadius, float AllowedArea);
	void WriteMask() const;
	void SwapToDamagedMaterials();
	void RestoreIntactMaterials();
	void OnDestructibleOwnerDied();
	void OnBuildingOwnerDied();
	// True while a building expansion shows its construction, placement or packing look.
	bool GetIsOwnerInTransition() const;

	/**
	 * @brief Boxes to spread collapse pieces over for shapes without a grid: the mesh's simple collision elements,
	 * so an L-shaped wall or a tower on legs throws pieces where it has material, not across empty bounds.
	 * @param OutBoxes Receives the element boxes in mesh space; the mesh bounds when it has no simple collision.
	 * @param OutWeights Receives each box's share of the pieces (its side area).
	 */
	void GetCollapseSampleBoxes(TArray<FBox>& OutBoxes, TArray<float>& OutWeights) const;

	bool GetIsValidWallMesh() const;
	bool GetIsValidDebrisSystem() const;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_WallMesh;

	// Oldest first; repairs close the newest.
	UPROPERTY()
	TArray<FWallHole> M_Holes;

	// Bit per grid cell, CellsPerGridFloat per word; written from FirstGridPrimitiveDataIndex.
	uint32 M_CellWords[WallHoleMask::MaxGridFloats] = {};

	// Knock-out order of the cells, so repairs close the newest first.
	TArray<int32> M_CellOrder;

	// The mesh the bounds and grid were read from.
	TWeakObjectPtr<const UStaticMesh> M_CachedShapeMesh;

	FBox M_MeshBounds = FBox(ForceInit);
	int32 M_Columns = 1;
	int32 M_Rows = 1;

	// Rough face area of each debris mesh (cm²), to scale a piece to the area it stands for.
	TArray<float> M_DebrisMeshAreas;

	float M_MaxOwnerHealth = 0.f;
	FVector M_LastShotDirection = FVector::ZeroVector;

	TSoftObjectPtr<UNiagaraSystem> M_DebrisSystem;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> M_LoadedDebrisSystem;

	FDelegateHandle M_OwnerDiedHandle;

	UPROPERTY()
	TWeakObjectPtr<UHealthComponent> M_OwnerHealth;

	FDelegateHandle M_OwnerHealedHandle;

	// Per slot, the material shown before the first hole swapped in the masked one; put back after a full repair.
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> M_IntactMaterials;

	bool bM_HasCollapsed = false;

	// Environment walls have no max health field: their max is the highest health seen.
	bool bM_HasKnownMaxHealth = false;
	void NoteOwnerHealth(const float Health)
	{
		M_MaxOwnerHealth = bM_HasKnownMaxHealth ? M_MaxOwnerHealth : FMath::Max(M_MaxOwnerHealth, Health);
	}
};
