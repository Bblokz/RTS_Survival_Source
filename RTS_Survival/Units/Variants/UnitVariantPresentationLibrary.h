// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnitVariantTypes.h"

#include "UnitVariantPresentationLibrary.generated.h"

class UDataTable;
class USlateBrushAsset;
struct FTrainingOption;

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

	/**
	 * @brief For screens without a training menu (faction selection): the DT_TrainingOptions table the training
	 * layout points at. The caller keeps it in a UPROPERTY so the row brushes stay loaded.
	 * @return Null when the training layout or its table is missing.
	 */
	static const UDataTable* LoadTrainingOptionsTable();

	/** @return The ImageBrush column of the unit's row, null when the unit has no row or no brush. */
	static USlateBrushAsset* FindTrainingTableImageBrush(const UDataTable* TrainingOptionsTable,
	                                                     const FTrainingOption& Unit);

	/** @return The Title column of the unit's row without rich text markup, empty when the unit has no row. */
	static FText FindTrainingTableTitle(const UDataTable* TrainingOptionsTable, const FTrainingOption& Unit);

	/** @return The text with every <Style>...</> rich text tag removed, for plain text blocks. */
	static FText StripRichTextMarkup(const FText& RichText);
};
