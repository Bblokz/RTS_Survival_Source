// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitVariantPresentationLibrary.h"

#include "Slate/SlateBrushAsset.h"

USlateBrushAsset* UUnitVariantPresentationLibrary::GetTrainingImageBrush(
	const FUnitVariantTrainingPresentation& Presentation, USlateBrushAsset* TableImageBrush)
{
	if (not Presentation.bOverrideImageBrush || not IsValid(Presentation.ImageBrush))
	{
		return TableImageBrush;
	}
	return Presentation.ImageBrush;
}

FText UUnitVariantPresentationLibrary::GetTrainingTitle(const FUnitVariantTrainingPresentation& Presentation,
                                                        const FText& TableTitle)
{
	return Presentation.bOverrideTitle ? Presentation.Title : TableTitle;
}

FText UUnitVariantPresentationLibrary::GetTrainingDescription(const FUnitVariantTrainingPresentation& Presentation,
                                                              const FText& TableDescription)
{
	return Presentation.bOverrideDescription ? Presentation.Description : TableDescription;
}

FText UUnitVariantPresentationLibrary::GetTrainingTooltip(const FUnitVariantTrainingPresentation& Presentation,
                                                          const FText& TableTooltip)
{
	return Presentation.bOverrideTooltip ? Presentation.Tooltip : TableTooltip;
}
