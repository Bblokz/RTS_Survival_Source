// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallDamageShared.h"

#include "WreckHoleDamageComponent.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;
class UStaticMeshComponent;
class UWreckHoleMaterialMap;
struct FDamageEvent;
struct FStreamableHandle;

namespace WreckHoleMask
{
	// Custom primitive data the `_WreckHoles` materials and M_WreckChar_Overlay read
	// (Tools/WreckHoles/make_wreck_holes_material.py): 6 holes as (mesh X, mesh Y, mesh Z, radius) from index 11.
	// Same slots as the wall holes (WallHoleMask), so one material snippet serves both.
	inline constexpr int32 FirstHolePrimitiveDataIndex = 11;
	inline constexpr int32 FloatsPerHole = 4;
	inline constexpr int32 MaxHoles = 6;
}

USTRUCT(BlueprintType)
struct FWreckHoleTuning
{
	GENERATED_BODY()

	// The struck surface is searched this far before and behind the reported impact (collision can sit off the mesh).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WreckHoles")
	float SurfaceProbeDistance = 150.f;

	// A hit inside an existing hole grows it instead of taking a slot: new radius = distance + this x hit radius.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WreckHoles")
	float GrowRadiusShare = 0.7f;

	// Holes smaller than this are not worth a slot.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WreckHoles")
	float MinHoleRadius = 8.f;

	// Surface area each flying shard stands for (cm²).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WreckHoles")
	float AreaPerDebrisPiece = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WreckHoles")
	int32 MaxPiecesPerHit = 8;

	// Shards leave the struck face towards the shooter.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WreckHoles")
	float DebrisSpeedScale = 0.9f;
};

USTRUCT()
struct FWreckHole
{
	GENERATED_BODY()

	// Sphere centre in the mesh's space, on the struck surface.
	UPROPERTY()
	FVector Centre = FVector::ZeroVector;

	UPROPERTY()
	float Radius = 0.f;
};

// Hole state of one wreck mesh (hull, turret, a part); every mesh has its own custom primitive data.
USTRUCT()
struct FWreckHoleMeshState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	// Oldest first; full slots merge the oldest into its nearest neighbour.
	UPROPERTY()
	TArray<FWreckHole> Holes;

	// The masked material per slot (null: that slot keeps its material and shows no holes).
	TArray<TSoftObjectPtr<UMaterialInterface>> DamagedMaterials;

	FBox LocalBounds = FBox(ForceInit);

	// Outer surface of the mesh bounds, scaled to the world (cm²); the health cap's share of the wreck.
	float FaceArea = 0.f;

	bool bHasRequestedDamagedMaterials = false;
};

// What one hit removed from a mesh; radii and centre in mesh space.
struct FWreckHoleCut
{
	float RemovedArea = 0.f;
	FVector MeshCentre = FVector::ZeroVector;
	// Zero for a new hole, the old radius for a grown one: shards only fly from the new ring.
	float InnerRadius = 0.f;
	float OuterRadius = 0.f;
};

// Where a hit lands on the wreck.
struct FWreckStruckSurface
{
	int32 MeshIndex = INDEX_NONE;
	FVector WorldPoint = FVector::ZeroVector;
	// Outward surface normal at the hit; shards leave along it.
	FVector WorldNormal = FVector::UpVector;
	// How far the reported hit is from the surface (splash explosions in the air).
	float DistanceFromHit = 0.f;
};

/**
 * @brief Procedural shell holes on vehicle wrecks and scavengeable vehicle parts (WallHitMasking.md option B on any
 * number of meshes): a hit cuts a noise-edged sphere hole into the struck mesh's custom primitive data, the mesh
 * swaps to its masked `_WreckHoles` materials, and metal shards fly off as pooled Niagara debris. The wreck's own
 * death (vertical sink, geometry collection) is untouched. ADestructableEnvActor forwards hits.
 * @note Set MaterialMap (DA_WreckHoleMaterials) and DebrisMeshes once on the family's parent Blueprint.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UWreckHoleDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWreckHoleDamageComponent();

	/**
	 * @brief Called by the owner after it applied the damage; cuts a hole where a big enough shell struck.
	 * @param DamageAmount Damage after the owner's reduction.
	 * @param DamageEvent A point event carries the impact; any other event uses the causer's position (splash).
	 * @param DamageCauser The projectile, bomb or ability actor; may be null.
	 * @param HealthBefore Owner health before this hit.
	 * @param HealthAfter Owner health after this hit.
	 */
	void OnOwnerDamaged(float DamageAmount, const FDamageEvent& DamageEvent, const AActor* DamageCauser,
	                    float HealthBefore, float HealthAfter);

	UFUNCTION(BlueprintPure, Category="WreckHoles")
	int32 GetHoleCount() const;

	UFUNCTION(BlueprintPure, Category="WreckHoles")
	int32 GetTrackedMeshCount() const { return M_MeshStates.Num(); }

	/**
	 * @brief Test hit without a projectile or health cap, for tuning.
	 * @param WorldLocation Impact point on (or near) the wreck.
	 * @param ShotDirection Direction the shell travelled.
	 * @param CalibreMm Calibre the hole is sized for.
	 * @return Surface area this hit removed, in m².
	 */
	UFUNCTION(BlueprintCallable, Category="WreckHoles|Debug")
	float Debug_FireTestHit(FVector WorldLocation, FVector ShotDirection, float CalibreMm);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Meshes that take holes; empty: every static mesh of the owner with at least one registered slot material.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WreckHoles")
	TArray<FName> MeshComponentNames;

	// Opaque slot material -> masked `_WreckHoles` material, shared by every wreck.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WreckHoles")
	TObjectPtr<UWreckHoleMaterialMap> MaterialMap;

	// Shards the missing metal flies off as, scaled to the area each stands for.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WreckHoles")
	TArray<TObjectPtr<UStaticMesh>> DebrisMeshes;

	// Calibre floor and hole size per mm; wreck defaults are smaller than the walls' (metal, not concrete).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WreckHoles")
	FWallChunkHoleSettings HoleSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WreckHoles")
	FWreckHoleTuning Tuning;

	// Mesh-space boxes holes may sit in (empty: anywhere). A hit outside every box makes no hole, and a hole stays
	// inside its box: bridges keep holes on the pillars, below the deck and rails.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WreckHoles")
	TArray<FBox> MeshSpaceHoleZones;

private:
	void BeginPlay_GatherMeshes();
	bool AddMeshState(UStaticMeshComponent& MeshComponent);
	void BeginPlay_CacheDebrisAreas();

	/**
	 * @brief Finds the wreck surface a hit landed on: a complex trace along the shot through each mesh, else the
	 * nearest mesh bounds.
	 * @param WorldHit Reported impact or explosion position.
	 * @param ShotDirection Shell direction; zero for splash.
	 * @param OutSurface Receives the mesh, point and normal.
	 * @return False when no tracked mesh is near the hit.
	 */
	bool FindStruckSurface(const FVector& WorldHit, const FVector& ShotDirection, FWreckStruckSurface& OutSurface) const;
	bool TraceMeshSurface(int32 MeshIndex, const FVector& WorldHit, const FVector& TraceDirection,
	                      FWreckStruckSurface& OutSurface) const;
	bool FindNearestBoundsSurface(const FVector& WorldHit, FWreckStruckSurface& OutSurface) const;
	FVector GetDirectionToNearestMesh(const FVector& WorldHit) const;

	/**
	 * @brief The point on a box's surface for a hit near it: the clamped point outside, the nearest face inside.
	 * @param Bounds Mesh-space bounds.
	 * @param LocalPoint Hit in mesh space.
	 * @param OutLocalNormal Receives the face's outward normal.
	 * @return The surface point in mesh space.
	 */
	static FVector GetBoundsSurfacePoint(const FBox& Bounds, const FVector& LocalPoint, FVector& OutLocalNormal);

	float ApplyHit(const FWreckStruckSurface& Surface, float HoleRadius, float AllowedArea);

	/**
	 * @brief The largest hole radius MeshSpaceHoleZones allow at a point.
	 * @param MeshPoint Hit in mesh space.
	 * @return Mesh-space radius; max float without zones, zero outside every zone.
	 */
	float GetZoneRadiusLimit(const FVector& MeshPoint) const;

	/**
	 * @brief Grows the hole the hit lands in, or takes a slot (merging or dropping the oldest when full).
	 * @param State The struck mesh.
	 * @param MeshPoint Hit in mesh space.
	 * @param Radius Hole radius in mesh space, after the health cap.
	 * @param AvailableArea World area (cm²) the health cap still allows; a grown hole stays within it.
	 * @return The cut: removed world area (zero for nothing), and the hole's centre and ring in mesh space.
	 */
	FWreckHoleCut CutHole(FWreckHoleMeshState& State, const FVector& MeshPoint, float Radius, float AvailableArea) const;
	void FreeOldestSlot(FWreckHoleMeshState& State) const;

	void AddShards(const FWreckStruckSurface& Surface, float InnerRadius, float OuterRadius,
	               TArray<FWallDebrisPiece>& OutPieces) const;

	float GetRemovedAreaEstimate() const;
	float GetTotalFaceArea() const;
	float GetAllowedRemovedArea(float HealthFractionAfter) const;
	float GetWorldScale(const FWreckHoleMeshState& State) const;

	void WriteMask(const FWreckHoleMeshState& State) const;
	void RequestDamagedMaterials(int32 MeshIndex);
	void ApplyDamagedMaterials(int32 MeshIndex);
	void ApplyDamagedMaterialToSlot(UStaticMeshComponent& Mesh, int32 SlotIndex,
	                                const TSoftObjectPtr<UMaterialInterface>& Damaged) const;

	bool GetIsValidDebrisSystem() const;

	UPROPERTY()
	TArray<FWreckHoleMeshState> M_MeshStates;

	// Rough face area of each debris mesh (cm²), to scale a shard to the area it stands for.
	TArray<float> M_DebrisMeshAreas;

	TSoftObjectPtr<UNiagaraSystem> M_DebrisSystem;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> M_LoadedDebrisSystem;

	// Keeps the damaged materials of hit meshes loaded.
	TArray<TSharedPtr<FStreamableHandle>> M_MaterialLoadHandles;

	// Environment actors have no max health field: their max is the highest health seen.
	float M_MaxOwnerHealth = 0.f;
	void NoteOwnerHealth(const float Health) { M_MaxOwnerHealth = FMath::Max(M_MaxOwnerHealth, Health); }
};
