// Copyright (C) Bas Blokzijl
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "TrainingPreviewSettings.generated.h"

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
};
