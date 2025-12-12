// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSActorSpline.generated.h"

class USplineComponent;
class UChildActorComponent;
class UWorld;

/**
 * @brief Place any actor class at each spline point (one actor per point), using each point's transform.
 *        Spawns preview child-actors in the editor, then rebuilds real actors at BeginPlay for runtime.
 *        Spawned actors keep full gameplay logic and can be destroyed safely at runtime.
 * @note Editor_ConvertSplineToPlacedActors: call in editor to bake actors into the level and delete this spline.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSActorSpline : public AActor
{
	GENERATED_BODY()

public:
	ARTSActorSpline();

	// Rebuild editor preview so designers see placements while editing.
	virtual void OnConstruction(const FTransform& Transform) override;

	// Runtime rebuild: removes editor preview and spawns actual level actors.
	virtual void BeginPlay() override;

	/** The spline that drives placement. Designers can add/move/rotate points. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline")
	USplineComponent* ActorSpline;

	/** The actor class to spawn at each spline point. Must be a subclass of AActor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	TSubclassOf<AActor> ActorClassToPlace;

	/** Optional per-actor rotation offset added to the spline point rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FRotator AdditionalRotationOffset = FRotator::ZeroRotator;

	/** If true, use the spline point scale for the spawned actor. Otherwise, use SpawnScaleOverride. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bUseSplinePointScale = true;

	/** Used when not inheriting point scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta=(EditCondition="!bUseSplinePointScale"))
	FVector SpawnScaleOverride = FVector(1.f, 1.f, 1.f);

	/** If true, create preview child-actors in the editor for instant visual feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Editor Preview")
	bool bSpawnPreviewInEditor = true;

	/** If true, attach spawned runtime actors to this actor (keeps outliner tidy). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bAttachSpawnedActorsToOwner = true;

	/** Returns how many runtime actors are currently tracked (valid + invalid weak refs). */
	UFUNCTION(BlueprintPure, Category = "Placement")
	int32 GetSpawnedActorCount() const { return M_SpawnedActors.Num(); }

#if WITH_EDITOR
	/** Convert the spline setup into real placed actors and then delete this spline actor. */
	UFUNCTION(CallInEditor, Category = "Spline|Actors")
	void Editor_ConvertSplineToPlacedActors();
#endif

	/** Default Z (Yaw) rotation in degrees to apply to newly added spline points in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	float DefaultNewPointYawDegrees = 0.f;

protected:
	// -- Tracked runtime actors (may be destructible at runtime) --
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_SpawnedActors;

	// -- Editor preview child-actors (only exist in editor/world construction) --
	UPROPERTY()
	TArray<TObjectPtr<UChildActorComponent>> M_PreviewChildActors;

	// Tracks how many spline points existed the last time we ran editor construction logic.
	// -1 means "uninitialized; do not touch existing points on the first pass".
	UPROPERTY(Transient)
	int32 M_LastSplinePointCount = -1;

private:
	// Build/destroy helpers (broken down for clarity and ≤60 lines).
	void BuildEditorPreview();
	void DestroyEditorPreview();

	void BuildRuntimeActors();
	void ClearRuntimeActors();


	// Transform helpers.
	FTransform MakeSpawnTransformAtPoint(int32 SplinePointIndex) const;

	// Safety/validator helpers (rule 0.5).
	bool GetIsValidActorSpline() const;
	bool GetIsValidActorClassToPlace() const;

	// BeginPlay split per your style guide.
	void BeginPlay_CleanupEditorPreview();
	void BeginPlay_SpawnActorsFromSpline();

	#if WITH_EDITOR
        void Editor_ApplyDefaultYawToNewPoints();
    #endif
};
