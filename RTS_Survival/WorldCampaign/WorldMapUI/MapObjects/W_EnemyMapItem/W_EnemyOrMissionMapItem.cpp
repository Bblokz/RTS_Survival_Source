// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_EnemyOrMissionMapItem.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_EnemyOrMissionMapItem::InitCardViewer(TWeakObjectPtr<UW_RewardCardsViewer> RewardCardsViewer)
{
	M_RewardCardsViewer = RewardCardsViewer;
	(void)EnsureIsValidRewardCardsViewer();
}

void UW_EnemyOrMissionMapItem::SetupEnemyWidget(const FEnemyOrMissionMapItemUIData& UIData,
                                                const FPrimaryReward& PrimaryReward, const FSecondaryReward& SecondaryReward)
{
}

bool UW_EnemyOrMissionMapItem::EnsureIsValidRewardCardsViewer() const
{
	if (not M_RewardCardsViewer.IsValid())
	{
		RTSFunctionLibrary::ReportError("No valid reward card viewer to display the reward cards in more detail!");
		return false;
	}
	return true;
	
}
