// Copyright (C) Bas Blokzijl
#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "TrainingPreviewHelper.generated.h"

class ANomadicVehicle;
class UStaticMesh;
class UMaterialInstanceDynamic;

/**
 * @brief State of the progress fill on the training preview: a dynamic instance of the progress material whose
 * BuildProgress scalar a 10 Hz timer moves along with the training of the previewed unit.
 */
USTRUCT()
struct FTrainingPreviewProgressState
{
	GENERATED_BODY()

	// Null while the preview mesh keeps its own material (no progress material configured for it).
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ProgressMaterial = nullptr;

	// Repeating timer that pushes the trainer's progress into ProgressMaterial while the preview shows.
	FTimerHandle UpdateTimerHandle;

	// Trained fraction seen at the previous update; the fill glides on from it during the running queue second.
	float LastTrainedFraction = -1.f;

	// World time at which LastTrainedFraction last changed.
	float LastFractionChangeTime = 0.f;
};

/**
 * @brief Lightweight helper that manages the cosmetic training preview mesh on a NomadicVehicle.
 * Spawns (once) a StaticMeshComponent at the "TrainingPreview" socket, async-loads the mesh per active TrainingOption,
 * and keeps it hidden/reset on cancel/complete or when the Nomad is a Truck (training disabled).
 * Meshes whose material has a progress material in UTrainingPreviewSettings::ProgressMaterials fill bottom-up with the
 * training progress of the previewed unit.
 */
USTRUCT()
struct RTS_SURVIVAL_API FTrainingPreviewHelper
{
	GENERATED_BODY()

public:
	/** @brief Ensure the preview component exists (attached to the building mesh at TrainingPreview socket). */
	void SetupTrainingPreviewIfNotExisting(ANomadicVehicle* NomadTraining);

	/**
	 * @brief Start or resume the current training preview. If the option hasn't changed, does nothing.
	 * @param StartedOption The active training option.
	 * @param Nomad The owning nomad.
	 */
	void OnStartOrResumeTraining(const FTrainingOption& StartedOption, ANomadicVehicle* Nomad);

	/** @brief Hide preview without clearing the cached option (used when training is disabled / Truck mode). */
	void HandleTrainingDisabledOrNomadTruck();

	/** @brief Called when the ACTIVE item was cancelled. Hides and resets the preview state. */
	void OnActiveTrainingCancelled(const FTrainingOption& CancelledOption);

	/** @brief Called when training completed. Hides and resets the preview state. */
	void OnTrainingCompleted(const FTrainingOption& CompletedOption);

	/** @brief Clear any pending async handle. Safe to call in BeginDestroy. */
	void ResetAsyncState();

	/**
	 * @brief The trainer advances its queue once per second; this extends the trained fraction by the part of the
	 * running second so the fill line glides and is full at the moment the unit completes.
	 * @param TrainedFraction Trained seconds / total seconds as the trainer reports it.
	 * @param TotalTrainingTimeSeconds Total training time of the unit; <= 0 shows TrainedFraction as is.
	 * @param SecondsSinceFractionChanged Time since TrainedFraction last changed; capped at one queue tick, so a
	 * paused queue holds the fill at most one tick ahead.
	 * @return The BuildProgress to show, 0 to 1.
	 */
	static float GetDisplayedBuildProgress(
		const float TrainedFraction,
		const int32 TotalTrainingTimeSeconds,
		const float SecondsSinceFractionChanged);

private:
	// Attached to building mesh socket when training; displays a preview of the unit being trained.
	UPROPERTY()
	UStaticMeshComponent* M_TrainingPreview = nullptr;

	// The active option for which we resolved/loaded a preview mesh.
	UPROPERTY()
	FTrainingOption M_ActivePreviewTrainingOption;

	// Cached soft mesh we requested for the active option.
	UPROPERTY()
	TSoftObjectPtr<UStaticMesh> M_CachedSoftMesh;

	// Cache the settings for faster repeated lookups.
	UPROPERTY()
	TWeakObjectPtr<const class UTrainingPreviewSettings> M_CachedSettings;

	// Handle for async load; not a UPROPERTY on purpose.
	TSharedPtr<struct FStreamableHandle> M_StreamableHandle;

	// The nomad that owns the preview; its trainer component reports the progress and its world runs the timer.
	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> M_Nomad;

	UPROPERTY()
	FTrainingPreviewProgressState M_ProgressState;

private:
	// --- small helpers (keep nesting low) ---

	/** @return true if the preview component is valid; logs once if not. */
	bool GetIsValidTrainingPreview() const;

	/** @return settings (from defaults), cached for reuse. */
	const class UTrainingPreviewSettings* GetSettingsCached();

	/** @brief Set visibility if the component exists. */
	void SetPreviewVisibility(bool bIsVisible) const;

	/** @brief Fetch soft mesh from settings; logs on failure. */
	TSoftObjectPtr<UStaticMesh> GetStaticMeshFromSettings(const FTrainingOption& TrainingOption);

	/** @brief Begin async load and set the mesh on completion (weak-captured). */
	void BeginAsyncLoadPreview(const TSoftObjectPtr<UStaticMesh>& SoftMesh, ANomadicVehicle* Nomad);

	/** @brief Fully reset current preview state and optional hide. */
	void ResetPreviewState(const bool bHide);

	/**
	 * @brief Swaps the loaded mesh's material for a dynamic instance of its progress material when the settings list
	 * one; otherwise the mesh keeps its own material.
	 * @param PreviewComponent The preview component that just got PreviewMesh.
	 * @param PreviewMesh The loaded preview mesh.
	 */
	void ApplyProgressMaterial(UStaticMeshComponent* PreviewComponent, const UStaticMesh* PreviewMesh);

	/** @brief Drops the progress material and the component's material overrides (they survive SetStaticMesh). */
	void ClearProgressMaterial();

	/** @return Whether the preview shows a progress material; no progress material is a normal case, not an error. */
	bool GetHasProgressMaterial() const;

	/** @brief Starts the 10 Hz progress timer unless it runs already; restarts the glide from the trainer's value. */
	void StartProgressUpdates();

	void StopProgressUpdates();

	/** @brief Timer body: reads the previewed unit's progress from the trainer and sets BuildProgress. */
	void UpdateProgressMaterial();

	bool bM_ShouldBeVisible = false;
};
