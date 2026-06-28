// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_EnemyOrMissionMapItem.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"


void UW_EnemyOrMissionMapItem::InitAuxiliaryWidgets(TWeakObjectPtr<UW_RewardCardsViewer> RewardCardsViewer,
	TWeakObjectPtr<UW_StrengthEstimation> StrengthEstimator)
{
	M_RewardCardsViewer = RewardCardsViewer;
	(void)EnsureIsValidRewardCardsViewer();
	M_StrengthEstimation = StrengthEstimator;
	(void)EnsureIsValidStrengthEstimation();
	
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

bool UW_EnemyOrMissionMapItem::EnsureIsValidStrengthEstimation() const
{
	if (not M_StrengthEstimation.IsValid())
	{
		RTSFunctionLibrary::ReportError("No valid Strength estimation widget (auxiliary)!");
		return false;
	}
	return true;
	
}
