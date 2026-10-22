// Copyright (C) Bas Blokzijl
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "TrainingPreviewSettings.generated.h"

class UMaterialInterface;

/**
 * @brief One material swap for the training preview: a preview mesh whose own material is PreviewMeshMaterial shows
 * ProgressMaterial instead, and its "BuildProgress" scalar fills the preview bottom-up as the unit trains.
 */
USTRUCT()
struct RTS_SURVIVAL_API FTrainingPreviewProgressMaterial
{
	GENERATED_BODY()

	// The material on the preview mesh asset, e.g. MI_WhileConstructing on the German previews.
	UPROPERTY(EditAnywhere, Category="Training Previews")
	TSoftObjectPtr<UMaterialInterface> PreviewMeshMaterial;

	// Shown instead while training; needs a scalar parameter "BuildProgress" (0 = nothing filled, 1 = all filled).
	UPROPERTY(EditAnywhere, Category="Training Previews")
	TSoftObjectPtr<UMaterialInterface> ProgressMaterial;
};

/**
 * @brief Designer mapping for training previews.
 * Designers map a TrainingOption name -> Soft UStaticMesh that is shown while training is active.
 * @note Keys must match UTrainingOptionLibrary::GetTrainingOptionName(UnitType, SubtypeValue).
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Training Previews"))
class RTS_SURVIVAL_API UTrainingPreviewSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTrainingPreviewSettings();

	/** Map: TrainingOptionName -> Preview mesh (soft). Example key: "Tank_Light" */
	UPROPERTY(Config, EditAnywhere, Category="Training Previews")
	TMap<FName, TSoftObjectPtr<UStaticMesh>> OptionNameToPreviewMesh;

	/**
	 * @brief Resolve the soft mesh for a TrainingOption via its canonical name.
	 * @param Option The training option (unit type + subtype).
	 * @return Soft mesh pointer (may be null if not found).
	 */
	TSoftObjectPtr<UStaticMesh> ResolvePreviewSoftMesh(const FTrainingOption& Option) const;

	/**
	 * Preview meshes whose material is listed here show the progress material instead, filled bottom-up while the
	 * unit trains. Meshes with an unlisted material keep their own material (e.g. the Soviet previews).
	 */
	UPROPERTY(Config, EditAnywhere, Category="Training Previews")
	TArray<FTrainingPreviewProgressMaterial> ProgressMaterials;

	/** @return The progress material for a preview mesh's own material; null when that material keeps its look. */
	TSoftObjectPtr<UMaterialInterface> ResolveProgressMaterial(const UMaterialInterface* PreviewMeshMaterial) const;

	/** @return Every configured progress material, so they load together with a preview mesh. */
	TArray<FSoftObjectPath> GetProgressMaterialPaths() const;
};
