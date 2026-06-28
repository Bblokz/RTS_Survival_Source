// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_RewardCardsViewer.h"

void UW_RewardCardsViewer::SetupCardViewer(const TArray<ERTSCard>& CardRewards)
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

		SetupRewardCard(RewardCardWidget, CardRewards[CardWidgetIndex]);
	}
}

void UW_RewardCardsViewer::ShowVisibleAnimation()
{
	BP_ShowVisibleAnimation();
}

TArray<UW_RTSCard*> UW_RewardCardsViewer::GetRewardCardWidgets() const
{
	return {BP_RTS_Card_1, BP_RTS_Card_2, BP_RTS_Card_3, BP_RTS_Card_4};
}

void UW_RewardCardsViewer::SetupRewardCard(UW_RTSCard* RewardCardWidget, const ERTSCard CardReward) const
{
	if (not IsValid(RewardCardWidget))
	{
		return;
	}

	RewardCardWidget->SetVisibility(ESlateVisibility::Visible);
	RewardCardWidget->InitializeCard(CardReward);
}
