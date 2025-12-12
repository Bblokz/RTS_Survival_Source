// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_TechItemDescription.h"

#include "Components/RichTextBlock.h"
#include "RTS_Survival/GameUI/CostWidget/W_CostDisplay.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"


void UW_TechItemDescription::InitTechItemDescription(const ETechnology Technology,
                                                     const TMap<ERTSResourceType, int32>& ResourceCosts,
                                                     const float TimeToResearch, const bool bHasBeenResearched)
{
	OnTechItemDescriptionUpdate(Technology, bHasBeenResearched);
	if (IsValid(CostDisplay))
	{
		CostDisplay->SetupCost(ResourceCosts);
	}
	InitResearchTime(TimeToResearch);
}

void UW_TechItemDescription::InitResearchTime(const float TimeToResearch)
{
	FString Text = "Time to research:";
	// time is in seconds, convert to minutes if over 60 seconds.
	if (TimeToResearch > 60)
	{
		Text += FRTSRichTextConverter::MakeStringRTSResource(FString::FromInt(TimeToResearch / 60) + " min",
		                                                    ERTSResourceRichText::DisplayAmount);
	}
	else
	{
		Text += FRTSRichTextConverter::MakeStringRTSResource(FString::FromInt(TimeToResearch) + " sec",
		                                                    ERTSResourceRichText::DisplayAmount);
	}
}

void UW_TechItemDescription::SetupAffectedUnitBlocks(const TArray<FText> AffectedUnitNames)
{
    // Maximum of 9 units displayed, 3 per block
    const int32 MaxUnitsToDisplay = 9;
    const int32 UnitsPerBlock = 3;

    // Initialize empty strings for the blocks
    FString BlockTexts[3] = { TEXT(""), TEXT(""), TEXT("") };

    const int32 TotalUnits = AffectedUnitNames.Num();

    // Determine the number of units to process
    int32 UnitsToProcess = FMath::Min(TotalUnits, MaxUnitsToDisplay);

    for (int32 i = 0; i < UnitsToProcess; ++i)
    {
        // Determine which block to put this unit in
        int32 BlockIndex = i / UnitsPerBlock;

        if (BlockIndex >= 3)
        {
            // Should not happen because UnitsToProcess <= MaxUnitsToDisplay
            break;
        }

        // Check if this is the last unit slot and there are more units to display
        if (i == MaxUnitsToDisplay - 1 && TotalUnits > MaxUnitsToDisplay)
        {
            // Display "more..." instead of the unit name
            FString MoreText = TEXT("more...");
            // Apply rich text formatting
            FString RichText = FRTSRichTextConverter::MakeRTSRich(MoreText, ERTSRichText::Text_Accent);
            // Append to current block without line break
            BlockTexts[BlockIndex] += RichText;
        }
        else
        {
            // Get the unit name
            FString UnitName = AffectedUnitNames[i].ToString();
        	FString RichPrefix = FRTSRichTextConverter::MakeRTSRich("-", ERTSRichText::Text_SubTitle);
            FString RichText = FRTSRichTextConverter::MakeRTSRich(UnitName, ERTSRichText::Text_Accent);
            // Append to current block with line break
            BlockTexts[BlockIndex] +=  RichPrefix + RichText + TEXT("\n");
        }
    }

    // Set the text of each block
    if (IsValid(AffectedUnits_1))
    {
        AffectedUnits_1->SetText(FText::FromString(BlockTexts[0]));
    }
    if (IsValid(AffectedUnits_2))
    {
        AffectedUnits_2->SetText(FText::FromString(BlockTexts[1]));
    }
    if (IsValid(AffectedUnits_3))
    {
        AffectedUnits_3->SetText(FText::FromString(BlockTexts[2]));
    }
}

