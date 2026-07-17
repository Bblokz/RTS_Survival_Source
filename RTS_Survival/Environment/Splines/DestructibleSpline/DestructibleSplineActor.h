// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ESplineDestructionType.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "RTS_Survival/Collapse/VerticalCollapse/RTSVerticalCollapseSettings.h"
#include "RTS_Survival/Environment/DestructableEnvActor/CrushDestructionTypes/ECrushDestructionType.h"
#include "DestructibleSplineActor.generated.h"

class USplineComponent;
class USplineMeshComponent;
class UGeometryCollection;
class UStaticMesh;
class UMaterialInterface;
class IRTSNavAgentInterface;

/**
 * @brief Authoring + runtime state for a single destructible piece on the spline.
 * Deform parameters are cached in spline-local space so a collapse proxy can
 * replicate the exact bent shape of the piece (no visual pop).
 */
USTRUCT()
struct FSplineDestructibleSegment
{
	GENERATED_BODY()

	// The deformed visual/collision component for this piece (one per segment).
	UPROPERTY()
	TObjectPtr<USplineMeshComponent> MeshComponent = nullptr;

	// Cached SetStartAndEnd parameters in spline-local space.
	FVector StartPos = FVector::ZeroVector;
	FVector StartTangent = FVector::ZeroVector;
	FVector EndPos = FVector::ZeroVector;
	FVector EndTangent = FVector::ZeroVector;

	// Remaining hit points of this piece; initialized from PieceHealth on build.
	float CurrentHealth = 0.f;

	// True once this piece has begun collapsing; guards against double-destruction
	// (e.g. crush overlap and weapon kill in the same frame).
	bool bIsCollapsing = false;
};

/**
 * @brief Spline system that tiles a (+X forward) mesh along its path where every piece is individually
 * collidable, optionally crushable by vehicles, and individually destructible.
 * - Collision per piece reuses FRTS_CollisionSetup::SetupDestructibleEnvActorMeshCollision, i.e. the exact
 *   same settings as crushable ADestructableEnvActor meshes.
 * - Crush-by-vehicle reuses the nav-agent gating of ADestructableEnvActor (ECrushDestructionType), but
 *   destroys only the overlapped piece instead of the whole actor.
 * - Destruction is per-system configured (all pieces share DestructionType and its settings): either a
 *   vertical collapse or a Chaos geometry-collection collapse, both executed by a temporary
 *   ASplinePieceCollapseProxy spawned at the destroyed piece's transform (with FCollapseFX).
 * @note Pieces do not affect navigation (matching the destructible env actors); add a nav modifier in BP if needed.
 */
UCLASS()
class RTS_SURVIVAL_API ADestructibleSplineActor : public AActor
{
	GENERATED_BODY()

public:
	ADestructibleSplineActor();

	// Rebuild editor preview whenever the spline or settings change.
	virtual void OnConstruction(const FTransform& Transform) override;

	/** The spline that drives placement; edit points in the level. Mesh runs forward along +X. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spline")
	TObjectPtr<USplineComponent> Spline;

	/** Mesh tiled along the spline (authored forward along +X). Simple collision recommended. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spline")
	TObjectPtr<UStaticMesh> PieceMesh = nullptr;

	/** Optional material override for slot 0 of every piece. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spline")
	TObjectPtr<UMaterialInterface> PieceMaterialOverride = nullptr;

	/**
	 * Hit points of every individual piece (per-system; all pieces share it). Weapon damage reduces a
	 * piece's health and the piece collapses when it reaches 0. Crush-by-vehicle bypasses health.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health", meta=(ClampMin="1.0"))
	float PieceHealth = 300.f;

	/** Flat damage reduction applied to every hit on a piece (mirrors ADestructableEnvActor::DamageReduction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health", meta=(ClampMin="0.0"))
	float PieceDamageReduction = 0.f;

	/**
	 * If true, pieces overlap vehicles (COLLISION_OBJ_PLAYER/ENEMY) and are destroyed when crushed,
	 * exactly like crushable destructible env actors. If false, pieces only block (no overlap events).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	bool bCrushableByVehicles = true;

	/** Which vehicles crush a piece on overlap (same gating as ADestructableEnvActor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision",
		meta=(EditCondition="bCrushableByVehicles"))
	ECrushDestructionType CrushDestructionType = ECrushDestructionType::AnyTank;

	/** How every piece of this spline collapses when destroyed (per-system; all pieces share it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction")
	ESplineDestructionType DestructionType = ESplineDestructionType::VerticalCollapse;

	/** Vertical collapse tuning (used when DestructionType == VerticalCollapse). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction",
		meta=(EditCondition="DestructionType==ESplineDestructionType::VerticalCollapse"))
	FRTSVerticalCollapseSettings VerticalCollapseSettings;

	/**
	 * Fractured Chaos asset for the piece (used when DestructionType == GeometryCollectionCollapse).
	 * Authored from the base (straight) piece mesh; async-loaded on first destruction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction",
		meta=(EditCondition="DestructionType==ESplineDestructionType::GeometryCollectionCollapse"))
	TSoftObjectPtr<UGeometryCollection> PieceGeometryCollection = nullptr;

	/** Radial strain force applied at the destroyed piece's center (geometry-collection collapse). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction",
		meta=(EditCondition="DestructionType==ESplineDestructionType::GeometryCollectionCollapse"))
	FCollapseForce CollapseForce;

	/** Rubble lifetime / keep-visible tuning (geometry-collection collapse). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction",
		meta=(EditCondition="DestructionType==ESplineDestructionType::GeometryCollectionCollapse"))
	FCollapseDuration CollapseDuration;

	/** SFX/VFX played at the destroyed piece's center when its collapse starts (both destruction types). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction")
	FCollapseFX CollapseFX;

	/** Editor button to force a rebuild after changing the spline or settings. */
	UFUNCTION(CallInEditor, Category="Spline")
	void RebuildSpline();

	/**
	 * @brief Destroys a single piece: removes it from the spline and spawns a collapse proxy at its
	 * transform that performs the configured destruction with FX.
	 * Safe to call twice for the same index (guarded); invalid indices are reported.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Destruction")
	void DestroySegment(int32 SegmentIndex);

	/**
	 * @brief Destroys the piece represented by the given component (e.g. from a weapon hit result).
	 * @param PieceComponent The piece's spline mesh component (FHitResult::Component).
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Destruction")
	void DestroySegmentByComponent(UPrimitiveComponent* PieceComponent);

	/**
	 * @brief Applies damage to a single piece (after PieceDamageReduction); destroys it at 0 health.
	 * Crush-by-vehicle does NOT use this: crushing always destroys instantly.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Health")
	void DamageSegment(int32 SegmentIndex, float DamageAmount);

	/**
	 * @brief Applies damage to the piece represented by the given component (e.g. from a custom BP hit).
	 * @param PieceComponent The piece's spline mesh component (FHitResult::Component).
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Health")
	void DamageSegmentByComponent(UPrimitiveComponent* PieceComponent, float DamageAmount);

	/** @return How many pieces have not been destroyed yet. */
	UFUNCTION(BlueprintPure, Category="Destruction")
	int32 GetNumRemainingPieces() const { return M_RemainingPieces; }

	/** @return Remaining health of the piece, or 0 if the index is invalid or the piece is destroyed. */
	UFUNCTION(BlueprintPure, Category="Health")
	float GetSegmentHealth(int32 SegmentIndex) const;

	/**
	 * @brief Routes engine damage to individual pieces (project convention of AInstancedDestrucablesEnvActor):
	 * - Point damage: resolved via the hit component when present, else the nearest alive piece to HitInfo
	 *   location (projectiles and bombs only set HitInfo.Location).
	 * - Radial damage: applied to every alive piece within the event's outer radius.
	 * - Plain damage events carry no spatial data and are ignored (returns 1.f, never 0 => no kill credit).
	 */
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Called after a piece took damage but survived (RemainingHealth > 0).
	UFUNCTION(BlueprintImplementableEvent, Category="Health")
	void BP_OnPieceDamaged(int32 SegmentIndex, float RemainingHealth);

	// Called after a piece started collapsing (the proxy is already running).
	UFUNCTION(BlueprintImplementableEvent, Category="Destruction")
	void BP_OnPieceCollapsed(int32 SegmentIndex);

	// Called when the last remaining piece started collapsing.
	UFUNCTION(BlueprintImplementableEvent, Category="Destruction")
	void BP_OnSplineFullyDestroyed();

private:
	// ------------------------- BUILD ----------------------------------

	/** Destroys all built segment components and clears the bookkeeping containers. */
	void ClearBuiltSegments();

	/** Builds one deformed, collidable spline mesh component per spline segment. */
	void BuildSpline();

	/** Creates, deforms and configures the piece component for segment SegmentIndex. */
	void BuildSegment(const int32 SegmentIndex);

	/** Applies the destructible-env-actor collision profile to a piece (bOverlapTanks = crushable). */
	void ApplyPieceCollision(USplineMeshComponent* Segment) const;

	/**
	 * @brief Reports (once) when the piece mesh asset has no usable collision data: without simple
	 * collision shapes or UseComplexAsSimple, spline pieces get NO physics body — they cannot block,
	 * be hit by weapons, or overlap vehicles (silent engine behavior, see USplineMeshComponent::GetBodySetup).
	 */
	void ValidatePieceMeshCollision();

	/**
	 * @brief Ensures the segment's deformed collision body exists; forces USplineMeshComponent::
	 * RecreateCollision when the engine skipped it and reports (once) if the body still cannot be built.
	 */
	void EnsureSegmentCollisionBuilt(USplineMeshComponent* SegmentComponent);

	// ------------------------- CRUSH DESTRUCTION ----------------------

	UFUNCTION()
	void HandlePieceBeginOverlap(UPrimitiveComponent* OverlappedComponent,
	                             AActor* OtherActor,
	                             UPrimitiveComponent* OtherComp,
	                             int32 OtherBodyIndex,
	                             bool bFromSweep,
	                             const FHitResult& SweepResult);

	/** Same nav-agent gate as ADestructableEnvActor::GetIsOverlap*Tank, selected by CrushDestructionType. */
	bool GetPassesCrushGate(AActor* OtherActor) const;

	// ------------------------- HEALTH / DAMAGE ------------------------

	/** Subtracts (reduced) damage from the piece's health; destroys the piece at 0. */
	void ApplyDamageToSegment(const int32 SegmentIndex, const float DamageAmount);

	/** @return Segment index for a point damage event (hit component, else nearest to hit location), or INDEX_NONE. */
	int32 ResolveSegmentFromPointDamage(const struct FPointDamageEvent& PointEvent) const;

	/** Applies DamageAmount to every alive piece within the radial event's outer radius. */
	void ApplyRadialDamageToSegments(const struct FRadialDamageEvent& RadialEvent, const float DamageAmount);

	/** @return Index of the alive piece whose start-end line is closest to WorldLocation, or INDEX_NONE. */
	int32 FindNearestAliveSegment(const FVector& WorldLocation) const;

	/** Reports (once per instance) damage that carried no spatial data and was ignored. */
	void ReportUnattributableDamageOnce(const TCHAR* Reason);

	// ------------------------- DESTRUCTION ----------------------------

	/** Validates the per-system destruction configuration before removing a piece. */
	bool GetIsValidDestructionConfig() const;

	/** Removes the piece component from the spline and disables its collision/overlap. */
	void RemovePieceComponent(FSplineDestructibleSegment& Segment);

	/**
	 * @brief Spawns the collapse proxy at the piece center (spline rotation/scale) and starts the
	 * configured destruction with deform parameters re-expressed relative to that center.
	 */
	void SpawnCollapseProxy(const FSplineDestructibleSegment& Segment);

	// ------------------------- STATE ----------------------------------

	// Per-piece bookkeeping; index == spline segment index.
	UPROPERTY()
	TArray<FSplineDestructibleSegment> M_Segments;

	// Piece component -> segment index for overlap handling and weapon-hit lookups.
	UPROPERTY()
	TMap<TObjectPtr<UPrimitiveComponent>, int32> M_SegmentIndexByComponent;

	// How many pieces are still standing.
	int32 M_RemainingPieces = 0;

	// One-shot diagnostic guards (reset per actor instance; deliberately not serialized).
	bool bM_ReportedMissingPieceCollision = false;
	bool bM_ReportedUnattributableDamage = false;
};
