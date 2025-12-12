// Copyright (C) Bas Blokzijl
#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "TrainingPreviewHelper.generated.h"

class ANomadicVehicle;
class UStaticMesh;

/**
 * @brief Lightweight helper that manages the cosmetic training preview mesh on a NomadicVehicle.
 * Spawns (once) a StaticMeshComponent at the "TrainingPreview" socket, async-loads the mesh per active TrainingOption,
 * and keeps it hidden/reset on cancel/complete or when the Nomad is a Truck (training disabled).
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

	bool bM_ShouldBeVisible = false;
};
