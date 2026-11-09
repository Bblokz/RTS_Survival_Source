// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WallDamageShared.h"

#include "WallHoleDamageComponent.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;
class UStaticMeshComponent;
struct FDamageEvent;

namespace WallHoleMask
{
	// Custom primitive data layout the procedural hole material reads (M_RusBunker_Concrete_WallHoles).
	// 8..10: grid cells, one bit per cell, 20 per float. 11..34: 8 holes as (mesh X, mesh Z, radius).
	inline constexpr int32 FirstGridPrimitiveDataIndex = 8;
	inline constexpr int32 CellsPerGridFloat = 20;
	inline constexpr int32 MaxGridFloats = 3;
	inline constexpr int32 MaxGridCells = CellsPerGridFloat * MaxGridFloats;
	inline constexpr int32 FirstHolePrimitiveDataIndex = 11;
	inline constexpr int32 FloatsPerHole = 3;
	inline constexpr int32 MaxHoles = 8;
}

USTRUCT(BlueprintType)
struct FWallHoleGridSettings
{
	GENERATED_BODY()

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

	// Face area each flying piece stands for (cm²).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	float AreaPerDebrisPiece = 4500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	int32 MaxPiecesPerHit = 12;

	// Spacing of the pieces thrown when the wall collapses.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	float CollapsePieceSpacing = 110.f;
};

USTRUCT()
struct FWallHole
{
	GENERATED_BODY()

	// Centre on the wall face (mesh X, mesh Z).
	UPROPERTY()
	FVector2D Centre = FVector2D::ZeroVector;

	UPROPERTY()
	float Radius = 0.f;
};

/**
 * @brief Procedural wall holes without extra geometry (WallHitMasking.md options A + B): a hit writes a round,
 * noise-edged hole into custom primitive data, big hits and overflowing holes knock out whole grid cells, and the
 * missing concrete flies off as pooled Niagara debris. Death throws the rest. ADestructableEnvActor forwards hits.
 * @note Set WallMeshComponentName, DamagedMaterials (M_RusBunker_Concrete_WallHoles instances) and DebrisMeshes.
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

	// Throws everything still standing and destroys the owner once the debris is gone. Runs once.
	UFUNCTION(BlueprintCallable, Category="WallHoles")
	void CollapseIntoPieces();

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

	// Concrete pieces the missing parts fly off as, scaled to the area each stands for.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	TArray<TObjectPtr<UStaticMesh>> DebrisMeshes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	FWallChunkHoleSettings HoleSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	FWallHoleGridSettings GridSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallHoles")
	FWallChunkCollapseSettings CollapseSettings;

private:
	void BeginPlay_FindWallMesh();
	void BeginPlay_CacheWallShape();
	void BeginPlay_CacheDebrisAreas();
	void BeginPlay_BindOwnerDeath();

	/**
	 * @brief Grows the hole the hit lands in, or takes a slot (baking the oldest hole into cells when full), and
	 * knocks out the hit cell for a big enough hole.
	 * @param FacePoint Hit on the wall face (mesh X/Z).
	 * @param Radius Hole radius after the health cap.
	 * @param OutPieces Receives the debris for the removed area.
	 * @return Face area removed (cm²), an estimate that ignores overlaps.
	 */
	float CutHole(const FVector2D& FacePoint, float Radius, TArray<FWallDebrisPiece>& OutPieces);
	float BakeOldestHoleIntoCells(TArray<FWallDebrisPiece>& OutPieces);
	float HideCell(int32 CellIndex, TArray<FWallDebrisPiece>& OutPieces);

	/**
	 * @brief Debris for a removed disc or ring on the face, one piece per AreaPerDebrisPiece.
	 * @param Centre Disc centre on the face.
	 * @param InnerRadius Zero for a new hole, the old radius for a grown one.
	 * @param OuterRadius Outer radius.
	 * @param OutPieces Receives the pieces.
	 */
	void AddDiscPieces(const FVector2D& Centre, float InnerRadius, float OuterRadius,
	                   TArray<FWallDebrisPiece>& OutPieces) const;
	void AddPiece(const FVector2D& FacePoint, float PieceArea, TArray<FWallDebrisPiece>& OutPieces) const;

	int32 GetCellIndexAt(const FVector2D& FacePoint) const;
	FVector2D GetCellCentre(int32 CellIndex) const;
	bool GetIsCellHidden(int32 CellIndex) const;
	bool GetIsFacePointRemoved(const FVector2D& FacePoint) const;
	float GetRemovedAreaEstimate() const;
	float GetAllowedRemovedArea(float HealthFractionAfter) const;

	float ApplyHit(const FVector& WorldHit, const FVector& ShotDirection, float HoleRadius, float AllowedArea);
	void WriteMask() const;
	void SwapToDamagedMaterials();
	void OnOwnerDied();

	bool GetIsValidWallMesh() const;
	bool GetIsValidDebrisSystem() const;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_WallMesh;

	UPROPERTY()
	TArray<FWallHole> M_Holes;

	// Bit per grid cell, CellsPerGridFloat per word; written from FirstGridPrimitiveDataIndex.
	uint32 M_CellWords[WallHoleMask::MaxGridFloats] = {};

	// Wall face in mesh space: X along the wall, Z up, Y through the slab.
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

	bool bM_HasSwappedToDamagedMaterials = false;
	bool bM_HasCollapsed = false;
};
