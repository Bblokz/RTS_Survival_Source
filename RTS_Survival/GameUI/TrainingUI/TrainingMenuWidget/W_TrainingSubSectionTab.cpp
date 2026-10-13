// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_TrainingSubSectionTab.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_TrainingSubSectionTab::InitSubSectionTab(UTrainingMenuManager* TrainingMenuManager,
                                                 const int32 SubSectionIndex)
{
	M_TrainingMenuManager = TrainingMenuManager;
	M_SubSectionIndex = SubSectionIndex;
}

void UW_TrainingSubSectionTab::UpdateSubSectionTab(const FText& DisplayName, const int32 QueuedCount,
                                                   const bool bIsSelected)
{
	M_TabText->SetText(DisplayName);
	M_TabText->SetColorAndOpacity(FSlateColor(bIsSelected ? SelectedTextColor : UnselectedTextColor));
	M_TabButton->SetBackgroundColor(bIsSelected ? SelectedButtonColor : UnselectedButtonColor);
	if (not M_TabCountText)
	{
		return;
	}
	M_TabCountText->SetText(FText::AsNumber(QueuedCount));
	M_TabCountText->SetVisibility(QueuedCount > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UW_TrainingSubSectionTab::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	M_TabButton->OnClicked.AddDynamic(this, &UW_TrainingSubSectionTab::OnTabClicked);
}

bool UW_TrainingSubSectionTab::GetIsValidTrainingMenuManager() const
{
	if (M_TrainingMenuManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TrainingMenuManager",
		"GetIsValidTrainingMenuManager",
		this
	);

	return false;
}

void UW_TrainingSubSectionTab::OnTabClicked()
{
	if (not GetIsValidTrainingMenuManager())
	{
		return;
	}
	M_TrainingMenuManager->OnSubSectionTabClicked(M_SubSectionIndex);
}
