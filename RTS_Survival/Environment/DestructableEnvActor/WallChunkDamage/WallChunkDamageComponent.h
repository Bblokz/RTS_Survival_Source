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
 * @brief Fakes wall destruction without Chaos: the wall mesh is cut into chunks (UWallChunkLayout), a hit clips the
 * chunks around the impact through a masked material and throws them as pooled Niagara debris; death throws the rest.
 * Add it to a destructible actor whose static mesh carries a wall chunk layout; ADestructableEnvActor forwards hits.
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

	// Throws every chunk still standing and destroys the owner once the debris is gone. Runs once.
	UFUNCTION(BlueprintCallable, Category="WallChunks")
	void CollapseIntoChunks();

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallChunks")
	FWallChunkHoleSettings HoleSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WallChunks")
	FWallChunkCollapseSettings CollapseSettings;

private:
	void BeginPlay_FindChunkMesh();
	void BeginPlay_BindOwnerDeath();

	/**
	 * @brief Knocks out the intact chunks within the hole around a mesh-space impact, nearest first, up to the cap.
	 * @param MeshSpaceHit Impact in the chunk mesh's space.
	 * @param HoleRadius Radius on the wall face.
	 * @param bIsDirectHit Direct hits always take the nearest chunk once the hole is big enough.
	 * @param MaxHiddenChunks Cap from the owner's lost health.
	 * @param OutNewlyHidden Receives the chunks this hit knocked out.
	 */
	void HideChunksAroundHit(const FVector& MeshSpaceHit, float HoleRadius, bool bIsDirectHit,
	                         int32 MaxHiddenChunks, TArray<int32>& OutNewlyHidden);

	int32 GetMaxHiddenChunks(float HealthFractionAfter) const;

	bool GetIsChunkHidden(int32 ChunkIndex) const;
	void SetChunkHidden(int32 ChunkIndex);
	void WriteHiddenChunkMask() const;
	void SwapToDamagedMaterials();

	/**
	 * @brief Throws the chunks as debris: each flies off as its layout debris piece, scaled to its area.
	 * @param ChunkIndices Chunks just knocked out.
	 * @param HitLocation World centre the pieces spread from.
	 * @param ShotDirection Pieces fly out of the face the shell went through; zero picks a side per piece.
	 * @param SpeedScale Death throws harder than a hit.
	 */
	void ThrowChunkDebris(TConstArrayView<int32> ChunkIndices, const FVector& HitLocation,
	                      const FVector& ShotDirection, float SpeedScale) const;

	void OnOwnerDied();

	bool GetIsValidChunkMesh() const;
	bool GetIsValidLayout() const;
	bool GetIsValidDebrisSystem() const;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ChunkMesh;

	UPROPERTY()
	TObjectPtr<const UWallChunkLayout> M_Layout;

	UPROPERTY()
	TArray<FWallChunk> M_Chunks;

	// One word per 20 chunks; bit = chunk hidden. Written to custom primitive data from FirstMaskPrimitiveDataIndex.
	uint32 M_HiddenChunkWords[WallChunkMask::MaxMaskFloats] = {};

	int32 M_HiddenChunkCount = 0;

	// Highest owner health seen: the owner has no max health field, so lost health is measured from this.
	float M_MaxOwnerHealth = 0.f;

	FVector M_LastShotDirection = FVector::ZeroVector;
	FVector M_LastHitLocation = FVector::ZeroVector;

	TSoftObjectPtr<UNiagaraSystem> M_DebrisSystem;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> M_LoadedDebrisSystem;

	FDelegateHandle M_OwnerDiedHandle;

	bool bM_HasSwappedToDamagedMaterials = false;
	bool bM_HasCollapsed = false;
};
