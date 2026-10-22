// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnitVariantTypes.h"

#include "UnitVariantPresentationLibrary.generated.h"

class USlateBrushAsset;

/**
 * @brief Used by the training widget Blueprints right after they read their DT_TrainingOptions row: each function
 * returns the variant's value when its override is on and the table value otherwise, so the widgets keep one path.
 */
UCLASS()
class RTS_SURVIVAL_API UUnitVariantPresentationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @param Presentation FTrainingWidgetState::VariantPresentation of the slot.
	 * @param TableImageBrush The ImageBrush column of the slot's DT_TrainingOptions row.
	 * @return The brush to show; the variant brush is held by the variant data asset, so it is never collected.
	 */
	UFUNCTION(BlueprintPure, Category = "Unit Variant|Training UI")
	static USlateBrushAsset* GetTrainingImageBrush(const FUnitVariantTrainingPresentation& Presentation,
	                                               USlateBrushAsset* TableImageBrush);

	UFUNCTION(BlueprintPure, Category = "Unit Variant|Training UI")
	static FText GetTrainingTitle(const FUnitVariantTrainingPresentation& Presentation, const FText& TableTitle);

	UFUNCTION(BlueprintPure, Category = "Unit Variant|Training UI")
	static FText GetTrainingDescription(const FUnitVariantTrainingPresentation& Presentation,
	                                    const FText& TableDescription);

	UFUNCTION(BlueprintPure, Category = "Unit Variant|Training UI")
	static FText GetTrainingTooltip(const FUnitVariantTrainingPresentation& Presentation, const FText& TableTooltip);
};
