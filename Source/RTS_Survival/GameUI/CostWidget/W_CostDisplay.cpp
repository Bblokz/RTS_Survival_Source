// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_CostDisplay.h"

#include "Components/RichTextBlock.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

void UW_CostDisplay::SetupCost(const TMap<ERTSResourceType, int32>& ResourceCosts)
{
	if (IsValid(CostTextBlock))
	{
		const FString CostText = GetCostText(ResourceCosts, false);
		CostTextBlock->SetText(FText::FromString(CostText));
	}
	if (IsValid(BlueprintTextBlock))
	{
		const FString BlueprintCostText = GetCostText(ResourceCosts, true);
		BlueprintTextBlock->SetText(FText::FromString(BlueprintCostText));
	}
}


FString UW_CostDisplay::GetCostText(const TMap<ERTSResourceType, int32>& ResourceCosts,
                                    const bool bIsBlueprintCost) const
{
	FString RewardText;
	for (const auto Cost : ResourceCosts)
	{
		if (bIsBlueprintCost && !GetIsResourceOfBlueprintType(Cost.Key) || !bIsBlueprintCost && GetIsResourceOfBlueprintType(Cost.Key))
		{
			// If we want to display blueprint costs but the cost is for a non blueprint, skip.
			continue;
		}

		// Obtain the amount text and styling.
		FString Amount = FRTSRichTextConverter::MakeStringRTSResource(FString::FromInt(Cost.Value),
		                                                             ERTSResourceRichText::DisplayAmount);

		// Obtain the resource image.
		FString ResourceImage = FRTSRichTextConverter::GetResourceRichImage(Cost.Key);


		RewardText += ResourceImage + " " + Amount + "\n";
	}
	return RewardText;
}
