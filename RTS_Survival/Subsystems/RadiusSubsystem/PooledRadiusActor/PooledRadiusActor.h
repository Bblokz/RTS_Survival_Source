// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/ERTSRadiusType.h"
#include "PooledRadiusActor.generated.h"

class URadiusComp;

/**
 * @brief Lightweight pooled actor that owns a URadiusComp and is shown/hidden by the pool subsystem.
 *        Spawns with a root scene so the radius mesh can safely attach. Tracks in-use state, pool id,
 *        and last-used timestamp for LRU selection by the subsystem.
 */
UCLASS()
class RTS_SURVIVAL_API APooledRadiusActor : public AActor
{
	GENERATED_BODY()

public:
	APooledRadiusActor();

	// AActor
	virtual void BeginPlay() override;

	/**
	 * @brief Initializes the radius component and marks this actor as dormant (hidden and not in use).
	 * @param RadiusMesh The mesh used to visualize the radius.
	 * @param StartingRadius Default radius applied on init (can be overridden on Activate).
	 * @param UnitsPerScale Number of world units per 1.0 mesh scale (x/y).
	 * @param ZScale Constant Z scale applied to the mesh.
	 * @param RenderHeight Vertical offset applied to the mesh.
	 * @param bUseFullCircleMesh True if this actor uses a full circle mesh (needed for reuse decisions).
	 */
	UFUNCTION(BlueprintCallable, Category="Radius|Pool")
	void InitRadiusActor(UStaticMesh* RadiusMesh, float StartingRadius, float UnitsPerScale, float ZScale,
	                     float RenderHeight, bool bUseFullCircleMesh);

	/**
	 * @brief Activates the pooled actor at a world location, applies optional material and radius, and shows it.
	 * @param WorldLocation Where to place the actor.
	 * @param Radius The actual radius to visualize.
	 * @param Material Optional material to apply (can be null to keep existing/default).
	 * @param RadiusType The type associated with this activation for reuse matching.
	 * @param bUseFullCircleMesh True if this activation uses a full circle mesh.
	 * @param RadiusParameterName Parameter name used for full circle radius materials.
	 */
	UFUNCTION(BlueprintCallable, Category="Radius|Pool")
	void ActivateRadiusAt(const FVector& WorldLocation, float Radius, UMaterialInterface* Material, ERTSRadiusType RadiusType,
	                      bool bUseFullCircleMesh, FName RadiusParameterName);

	/**
	 * @brief Hides and deactivates this actor, detaches from any parent, and returns it to the pool.
	 */
	UFUNCTION(BlueprintCallable, Category="Radius|Pool")
	void DeactivateRadius();

	/**
	 * @brief Applies a new material to the underlying radius mesh (for dynamic type changes).
	 * @param NewMaterial The material to apply.
	 */
	UFUNCTION(BlueprintCallable, Category="Radius|Pool")
	void ApplyMaterial(UMaterialInterface* NewMaterial);

	/**
	 * @brief Convenience: attach this radius actor to another actor with a relative offset.
	 * @param TargetActor The actor to attach to.
	 * @param RelativeOffset Local-space offset after attachment.
	 */
	UFUNCTION(BlueprintCallable, Category="Radius|Pool")
	void AttachToTargetActor(AActor* TargetActor, const FVector& RelativeOffset);

	/** Pool API expected by the subsystem. */
	inline void SetPoolId(const int32 InId) { M_PoolId = InId; }
	inline int32 GetPoolId() const { return M_PoolId; }
	inline bool GetIsInUse() const { return bM_InUse; }
	inline float GetLastUsedWorldSeconds() const { return M_LastUsedWorldSeconds; }
	inline ERTSRadiusType GetRadiusType() const { return M_RadiusType; }
	inline bool GetUsesFullCircleMesh() const { return bM_UsesFullCircleMesh; }

private:
	/** Validity helper as per rule 0.5. */
	bool GetIsValidRadiusComp() const;
	bool GetIsValidRootScene() const;

private:
	/** Root scene so attached components (and radius mesh) always have a valid parent. */
	UPROPERTY(VisibleDefaultsOnly, Category="Radius")
	TObjectPtr<USceneComponent> M_RootScene = nullptr;

	/** The radius component that owns the mesh and scaling logic. */
	UPROPERTY(VisibleDefaultsOnly, Category="Radius")
	TObjectPtr<URadiusComp> M_RadiusComp = nullptr;

	// Current pool id; INDEX_NONE when not bound in the subsystem map.
	int32 M_PoolId = INDEX_NONE;

	// Pooling state (true when handed out and visible/active).
	bool bM_InUse = false;

	// Track the last radius type to enable reuse without redundant material changes.
	ERTSRadiusType M_RadiusType = ERTSRadiusType::None;

	// Track which mesh family this actor currently uses.
	bool bM_UsesFullCircleMesh = false;

	// Timestamp when last activated (world seconds); used for LRU fallback.
	float M_LastUsedWorldSeconds = 0.0f;
};
