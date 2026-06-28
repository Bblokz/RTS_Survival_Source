// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_EnemyOrMissionMapItem.h"

#include "EnemyMapItemUIData/FEnemyMapItemUIData.h"
#include "StrengthEstimation/W_StrengthEstimation.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_MissionReward/W_MissionReward.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_MissionReward/W_RewardCardsViewer/W_RewardCardsViewer.h"

void UW_EnemyOrMissionMapItem::InitAuxiliaryWidgets(TWeakObjectPtr<UW_RewardCardsViewer> RewardCardsViewer,
	TWeakObjectPtr<UW_StrengthEstimation> StrengthEstimator)
{
	M_RewardCardsViewer = RewardCardsViewer;
	(void)EnsureIsValidRewardCardsViewer();
	M_StrengthEstimation = StrengthEstimator;
	(void)EnsureIsValidStrengthEstimation();
	OnStrengthButtonUnhovered();
	OnMissionRewardUnhovered();
}

void UW_EnemyOrMissionMapItem::SetupEnemyWidget(const FEnemyOrMissionMapItemUIData& UIData,
                                                const FPrimaryReward& PrimaryReward, const FSecondaryReward& SecondaryReward)
{
	SetTextIfValid(M_ItemRichTitle, UIData.TitleText);
	SetTextIfValid(M_RichStrengthText, UIData.StrengthPercentageText);
	SetTextIfValid(M_ItemRichDescription, UIData.DescriptionText);
	SetTextIfValid(M_PrimaryObjDesc, UIData.PrimaryObjectiveText);
	SetTextIfValid(M_SecondaryObjDesc, UIData.SecondaryObjectiveText);
	SetTextIfValid(M_OnDefeatRichText, UIData.OnDefeatText);

	if (GetIsValidMissionReward())
	{
		W_BP_MissionReward->SetupReward(PrimaryReward, SecondaryReward);
	}
	if (EnsureIsValidRewardCardsViewer())
	{
		M_RewardCardsViewer->SetupCardViewer(PrimaryReward.CardRewards);
	}
	if (EnsureIsValidStrengthEstimation())
	{
		M_StrengthEstimation->SetupStrengthEstimation(UIData.StrengthEstimation);
	}
}

void UW_EnemyOrMissionMapItem::ShowVisibleAnimation()
{
	BP_ShowVisibleAnimation();
}

void UW_EnemyOrMissionMapItem::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (GetIsValidStrengthButton())
	{
		M_StrengthButton->OnHovered.AddDynamic(this, &UW_EnemyOrMissionMapItem::OnStrengthButtonHovered);
		M_StrengthButton->OnUnhovered.AddDynamic(this, &UW_EnemyOrMissionMapItem::OnStrengthButtonUnhovered);
	}
	if (GetIsValidMissionReward())
	{
		W_BP_MissionReward->OnMissionRewardHovered().AddUObject(this, &UW_EnemyOrMissionMapItem::OnMissionRewardHovered);
		W_BP_MissionReward->OnMissionRewardUnhovered().AddUObject(this, &UW_EnemyOrMissionMapItem::OnMissionRewardUnhovered);
	}
}

void UW_EnemyOrMissionMapItem::OnStrengthButtonHovered()
{
	if (not EnsureIsValidStrengthEstimation())
	{
		return;
	}
	M_StrengthEstimation->SetVisibility(ESlateVisibility::Visible);
	M_StrengthEstimation->ShowVisibleAnimation();
}

void UW_EnemyOrMissionMapItem::OnStrengthButtonUnhovered()
{
	if (not M_StrengthEstimation.IsValid())
	{
		return;
	}
	M_StrengthEstimation->SetVisibility(ESlateVisibility::Collapsed);
}

void UW_EnemyOrMissionMapItem::OnMissionRewardHovered()
{
	if (not EnsureIsValidRewardCardsViewer())
	{
		return;
	}
	M_RewardCardsViewer->SetVisibility(ESlateVisibility::Visible);
	M_RewardCardsViewer->ShowVisibleAnimation();
}

void UW_EnemyOrMissionMapItem::OnMissionRewardUnhovered()
{
	if (not M_RewardCardsViewer.IsValid())
	{
		return;
	}
	M_RewardCardsViewer->SetVisibility(ESlateVisibility::Collapsed);
}

void UW_EnemyOrMissionMapItem::SetTextIfValid(URichTextBlock* RichTextBlock, const FText& Text) const
{
	if (not IsValid(RichTextBlock))
	{
		return;
	}
	RichTextBlock->SetText(Text);
}

bool UW_EnemyOrMissionMapItem::GetIsValidStrengthButton() const
{
	if (IsValid(M_StrengthButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_StrengthButton",
		"UW_EnemyOrMissionMapItem::GetIsValidStrengthButton",
		this
	);
	return false;
}

bool UW_EnemyOrMissionMapItem::GetIsValidMissionReward() const
{
	if (IsValid(W_BP_MissionReward))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"W_BP_MissionReward",
		"UW_EnemyOrMissionMapItem::GetIsValidMissionReward",
		this
	);
	return false;
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
