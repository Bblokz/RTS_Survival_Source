// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_TrainingBuildingColumn.h"

#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TrainingMenuWidgetHelpers.h"

void UW_TrainingBuildingEntry::InitBuildingEntry(UTrainingMenuManager* TrainingMenuManager, const int32 EntryIndex)
{
	M_TrainingMenuManager = TrainingMenuManager;
	M_EntryIndex = EntryIndex;
	// Players count buildings from one.
	M_EntryText->SetText(FText::AsNumber(EntryIndex + 1));
}

void UW_TrainingBuildingEntry::UpdateBuildingEntry(const FTrainingBuildingEntryState& EntryState)
{
	M_EntryButton->SetBackgroundColor(EntryState.bIsShownBuilding
		                                  ? ShownBuildingButtonColor
		                                  : OtherBuildingButtonColor);
	// Hidden instead of collapsed, so that the entry keeps its size when training starts and stops.
	M_TrainingProgressBar->SetVisibility(EntryState.bIsTraining
		                                     ? ESlateVisibility::HitTestInvisible
		                                     : ESlateVisibility::Hidden);
	M_TrainingProgressBar->SetPercent(EntryState.TrainingProgress);
}

void UW_TrainingBuildingEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	M_EntryButton->OnClicked.AddDynamic(this, &UW_TrainingBuildingEntry::OnEntryClicked);
}

bool UW_TrainingBuildingEntry::GetIsValidTrainingMenuManager() const
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

void UW_TrainingBuildingEntry::OnEntryClicked()
{
	if (not GetIsValidTrainingMenuManager())
	{
		return;
	}
	M_TrainingMenuManager->OnBuildingColumnEntryClicked(M_EntryIndex);
}

void UW_TrainingBuildingColumn::InitBuildingColumn(UTrainingMenuManager* TrainingMenuManager)
{
	for (int32 EntryIndex = 0; EntryIndex < M_BuildingEntries.Num(); ++EntryIndex)
	{
		M_BuildingEntries[EntryIndex]->InitBuildingEntry(TrainingMenuManager, EntryIndex);
		M_BuildingEntries[EntryIndex]->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UW_TrainingBuildingColumn::UpdateBuildingEntries(const TArray<FTrainingBuildingEntryState>& EntryStates)
{
	for (int32 EntryIndex = 0; EntryIndex < M_BuildingEntries.Num(); ++EntryIndex)
	{
		UW_TrainingBuildingEntry* BuildingEntry = M_BuildingEntries[EntryIndex];
		if (not EntryStates.IsValidIndex(EntryIndex))
		{
			BuildingEntry->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		BuildingEntry->SetVisibility(ESlateVisibility::Visible);
		BuildingEntry->UpdateBuildingEntry(EntryStates[EntryIndex]);
	}
}

void UW_TrainingBuildingColumn::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	TrainingMenuWidgets::CollectWidgetsInPanel(M_EntryPanel, M_BuildingEntries);
}
