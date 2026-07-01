// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_MissionReward.h"

#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

namespace
{
	FText BuildBlueprintText(const FUnitCost& BlueprintRewards)
	{
		if (BlueprintRewards.ResourceCosts.Num() == 0)
		{
			return FText::GetEmpty();
		}

		TArray<FString> RewardTexts;
		for (const TPair<ERTSResourceType, int32>& BlueprintReward : BlueprintRewards.ResourceCosts)
		{
			const UEnum* ResourceEnum = StaticEnum<ERTSResourceType>();
			const FString ResourceName = IsValid(ResourceEnum)
				? ResourceEnum->GetNameStringByValue(static_cast<int64>(BlueprintReward.Key))
				: TEXT("Blueprint");
			RewardTexts.Add(FString::Printf(TEXT("<DisplayAmount>+%d</> %s"), BlueprintReward.Value, *ResourceName));
		}

		return FText::FromString(FString::Join(RewardTexts, TEXT(" ")));
	}
}

void UW_MissionReward::SetupReward(const FPrimaryReward& PrimaryReward,
                                   const FSecondaryReward& SecondaryReward,
                                   const ERTSFaction PlayerFaction)
{
	SetupRewardCards(PrimaryReward.GetCardRewardsForFaction(PlayerFaction));

	if (IsValid(M_RadixiteBonusRichText))
	{
		M_RadixiteBonusRichText->SetText(FText::FromString(FString::Printf(TEXT("<DisplayAmount>+%d</><img id=\"Radixite\"/><Radixite>Radixite</>"), PrimaryReward.RadixiteReward)));
	}
	if (IsValid(M_CommandExpBonusRichText))
	{
		M_CommandExpBonusRichText->SetText(FText::FromString(FString::Printf(TEXT("<DisplayAmount>+%d</><Exp>Exp</>"), PrimaryReward.CommanderXpReward)));
	}
	if (IsValid(M_BlueprintBonuses))
	{
		M_BlueprintBonuses->SetText(BuildBlueprintText(PrimaryReward.Blueprints));
	}
	if (IsValid(M_SecondaryBonuses))
	{
		M_SecondaryBonuses->SetText(BuildBlueprintText(SecondaryReward.SecondaryReward));
	}
}

void UW_MissionReward::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	M_OnMissionRewardHovered.Broadcast();
}

void UW_MissionReward::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	M_OnMissionRewardUnhovered.Broadcast();
}

TArray<UW_RTSCard*> UW_MissionReward::GetRewardCardWidgets() const
{
	return {BP_RTS_Card_1, BP_RTS_Card_2, BP_RTS_Card_3, BP_RTS_Card_4};
}

void UW_MissionReward::SetupRewardCards(const TArray<ERTSCard>& CardRewards) const
{
	const TArray<UW_RTSCard*> RewardCardWidgets = GetRewardCardWidgets();
	for (int32 CardWidgetIndex = 0; CardWidgetIndex < RewardCardWidgets.Num(); ++CardWidgetIndex)
	{
		UW_RTSCard* RewardCardWidget = RewardCardWidgets[CardWidgetIndex];
		if (not IsValid(RewardCardWidget))
		{
			continue;
		}
		if (not CardRewards.IsValidIndex(CardWidgetIndex))
		{
			RewardCardWidget->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		RewardCardWidget->SetVisibility(ESlateVisibility::Visible);
		RewardCardWidget->InitializeCard(CardRewards[CardWidgetIndex]);
	}
}
