// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WallChunkLayout.h"
#include "WallDamageShared.h"

#include "WallChunkDamageComponent.generated.h"

class UNiagaraSystem;
class UStaticMeshComponent;
struct FDamageEvent;

/**
 * @brief Fakes destruction without Chaos (WallHitMasking.md option D): the mesh's concrete is cut into chunks
 * (UWallChunkLayout), a hit clips the chunks around the impact through a masked material and throws them as pooled
 * Niagara debris. Works on walls (ADestructableEnvActor) and buildings (AHPActorObjectsMaster: building expansions,
 * bunkers), which forward hits and repairs; repairs bring back the newest chunks first.
 * @note On buildings, call ThrowDeathPieces from the death graph next to VerticalDestruction for the shot-out pieces.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UWallChunkDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWallChunkDamageComponent();

	/**
	 * @brief Called by the owner after it applied the damage; knocks out the chunks around the hit.
	 * @param DamageAmount Damage after the owner's reduction.
	 * @param DamageEvent A point event carries the impact; any other event uses the causer's position (splash).
	 * @param DamageCauser The projectile, bomb or ability actor; may be null.
	 * @param HealthBefore Owner health before this hit.
	 * @param HealthAfter Owner health after this hit.
	 */
	void OnOwnerDamaged(float DamageAmount, const FDamageEvent& DamageEvent, const AActor* DamageCauser,
	                    float HealthBefore, float HealthAfter);

	/**
	 * @brief Repairs bring chunks back, newest first, until the missing ones fit the health again.
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
	 * @brief Throws every chunk still standing. Runs once.
	 * @param bHideAndDestroyOwner Walls vanish and die after the debris; buildings keep their mesh for their own death.
	 */
	UFUNCTION(BlueprintCallable, Category="WallChunks")
	void CollapseIntoChunks(bool bHideAndDestroyOwner = true);

	/**
	 * @brief Death of a building that sinks (VerticalDestruction): a burst of its own chunks is shot out of the mesh,
	 * those chunks disappear from it, the rest of the building stays and sinks.
	 * @param MaxPieces How many chunks fly; picked at random among the standing ones.
	 * @param SpeedScale Throw speed relative to a hit.
	 * @return Chunks thrown.
	 */
	UFUNCTION(BlueprintCallable, Category="WallChunks")
	int32 ThrowDeathPieces(int32 MaxPieces = 24, float SpeedScale = 1.4f);

	UFUNCTION(BlueprintPure, Category="WallChunks")
	int32 GetHiddenChunkCount() const { return M_HiddenChunkCount; }

	UFUNCTION(BlueprintPure, Category="WallChunks")
	int32 GetChunkCount() const { return M_Chunks.Num(); }

	/**
	 * @brief Test hit without a projectile, for tuning and the headless check.
	 * @param WorldLocation Impact point on the wall.
	 * @param ShotDirection Direction the shell travelled.
	 * @param CalibreMm Calibre the hole is sized for.
	 * @return Number of chunks this hit knocked out.
	 */
	UFUNCTION(BlueprintCallable, Category="WallChunks|Debug")
	int32 Debug_FireTestHit(FVector WorldLocation, FVector ShotDirection, float CalibreMm);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// The mesh component carrying the chunked mesh. None: the first static mesh component whose mesh has a chunk
	// layout (walls). Buildings name it (BuildingMeshComponent), because their mesh only gets the chunked one once
	// construction finishes.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallChunks")
	FName ChunkMeshComponentName = NAME_None;

	// Per-Blueprint masked materials by slot, for variants that override the mesh's look (Soviet concrete). A valid
	// entry wins over the layout's damaged material for that slot; empty or None keeps the layout's.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallChunks")
	TArray<TObjectPtr<UMaterialInterface>> DamagedMaterialOverrides;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallChunks")
	FWallChunkHoleSettings HoleSettings;

	// bCollapseIntoChunksOnDeath: walls throw everything and vanish; buildings throw everything and keep their mesh.
	// Bunkers turn it off and call ThrowDeathPieces from their death graph instead.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallChunks")
	FWallChunkCollapseSettings CollapseSettings;

private:
	void BeginPlay_BindOwnerDeath();

	// Finds the chunk mesh and its layout; buildings get the chunked mesh only after construction.
	bool EnsureLayoutIsCurrent();
	void CacheLayout(UStaticMeshComponent& MeshComponent, const UWallChunkLayout& Layout);

	/**
	 * @brief Knocks out the intact chunks within the hole around a mesh-space impact, nearest first, up to the cap.
	 * @param MeshSpaceHit Impact in the chunk mesh's space.
	 * @param HoleRadius Radius of the hole (a sphere around the impact).
	 * @param bIsDirectHit Direct hits always take the nearest chunk once the hole is big enough.
	 * @param MaxHiddenChunks Cap from the owner's lost health.
	 * @param OutNewlyHidden Receives the chunks this hit knocked out.
	 */
	void HideChunksAroundHit(const FVector& MeshSpaceHit, float HoleRadius, bool bIsDirectHit,
	                         int32 MaxHiddenChunks, TArray<int32>& OutNewlyHidden);

	int32 GetMaxHiddenChunks(float HealthFractionAfter) const;

	bool GetIsChunkHidden(int32 ChunkIndex) const;
	void SetChunkHidden(int32 ChunkIndex);
	void SetChunkVisible(int32 ChunkIndex);
	void WriteHiddenChunkMask() const;
	void SwapToDamagedMaterials();
	// True while a building expansion shows its construction, placement or packing look.
	bool GetIsOwnerInTransition() const;

	/**
	 * @brief Throws the chunks as debris: each flies off as its layout debris piece, scaled to its area.
	 * @param ChunkIndices Chunks just knocked out.
	 * @param HitLocation World centre the pieces spread from.
	 * @param ShotDirection Pieces fly out of the face the shell went through; zero picks a side per piece.
	 * @param SpeedScale Death throws harder than a hit.
	 */
	void ThrowChunkDebris(TConstArrayView<int32> ChunkIndices, const FVector& HitLocation,
	                      const FVector& ShotDirection, float SpeedScale) const;

	void OnDestructibleOwnerDied();
	void OnBuildingOwnerDied();

	bool GetIsValidChunkMesh() const;
	bool GetIsValidLayout() const;
	bool GetIsValidDebrisSystem() const;

	void NoteOwnerHealth(const float Health)
	{
		M_MaxOwnerHealth = bM_HasKnownMaxHealth ? M_MaxOwnerHealth : FMath::Max(M_MaxOwnerHealth, Health);
	}

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ChunkMesh;

	UPROPERTY()
	TObjectPtr<const UWallChunkLayout> M_Layout;

	UPROPERTY()
	TArray<FWallChunk> M_Chunks;

	// One word per 20 chunks; bit = chunk hidden. Written to custom primitive data from FirstMaskPrimitiveDataIndex.
	uint32 M_HiddenChunkWords[WallChunkMask::MaxMaskFloats] = {};

	// Hide order, so repairs bring back the newest chunks first.
	TArray<int32> M_HiddenOrder;

	int32 M_HiddenChunkCount = 0;

	// Radius of a typical chunk (from the layout's mean area): a direct hit takes the nearest chunk only within it.
	float M_TypicalChunkRadius = 75.f;

	FBox M_MeshBounds = FBox(ForceInit);

	float M_MaxOwnerHealth = 0.f;
	bool bM_HasKnownMaxHealth = false;

	FVector M_LastShotDirection = FVector::ZeroVector;
	FVector M_LastHitLocation = FVector::ZeroVector;

	TSoftObjectPtr<UNiagaraSystem> M_DebrisSystem;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> M_LoadedDebrisSystem;

	FDelegateHandle M_OwnerDiedHandle;

	bool bM_HasCollapsed = false;
};
