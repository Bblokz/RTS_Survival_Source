// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_TrainingMenu.h"

#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingLayout/TrainingLayoutSubsystem.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingUIWidget/W_TrainingItem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_TrainingQuickNavButton.h"
#include "W_TrainingSubSectionTab.h"

namespace TrainingMenuWidgets
{
	// Collects in widget tree order, also through nested panels such as one horizontal box per row.
	template <typename TWidgetType>
	void CollectWidgetsInPanel(UPanelWidget* Panel, TArray<TWidgetType*>& OutWidgets)
	{
		if (not IsValid(Panel))
		{
			return;
		}
		for (UWidget* ChildWidget : Panel->GetAllChildren())
		{
			if (TWidgetType* TypedChild = Cast<TWidgetType>(ChildWidget))
			{
				OutWidgets.Add(TypedChild);
				continue;
			}
			CollectWidgetsInPanel(Cast<UPanelWidget>(ChildWidget), OutWidgets);
		}
	}

	void MakeWidgetFillHorizontalBox(const UWidget* Widget)
	{
		UHorizontalBoxSlot* HorizontalBoxSlot = Cast<UHorizontalBoxSlot>(Widget->Slot);
		if (not IsValid(HorizontalBoxSlot))
		{
			return;
		}
		HorizontalBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HorizontalBoxSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UW_TrainingMenu::InitTrainingMenu(UTrainingMenuManager* TrainingMenuManager)
{
	for (int32 TabIndex = 0; TabIndex < M_SubSectionTabs.Num(); ++TabIndex)
	{
		M_SubSectionTabs[TabIndex]->InitSubSectionTab(TrainingMenuManager, TabIndex);
	}
	InitTrainingMenu_SetupQuickNavButtons(TrainingMenuManager);
}

void UW_TrainingMenu::UpdateSubSectionTabs(const TArray<FTrainingSubSectionTabState>& TabStates,
                                           const int32 SelectedSubSectionIndex)
{
	if (TabStates.Num() > M_SubSectionTabs.Num())
	{
		RTSFunctionLibrary::ReportError("The trainer has more sub sections than the training menu has tabs."
			"\n Sub sections: " + FString::FromInt(TabStates.Num()) +
			"\n Tabs: " + FString::FromInt(M_SubSectionTabs.Num()) +
			"\n See UW_TrainingMenu::UpdateSubSectionTabs.");
	}

	for (int32 TabIndex = 0; TabIndex < M_SubSectionTabs.Num(); ++TabIndex)
	{
		UW_TrainingSubSectionTab* Tab = M_SubSectionTabs[TabIndex];
		if (not TabStates.IsValidIndex(TabIndex))
		{
			// Collapsed tabs take no space, which lets the remaining tabs fill the row.
			Tab->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		Tab->SetVisibility(ESlateVisibility::Visible);
		Tab->UpdateSubSectionTab(TabStates[TabIndex].DisplayName, TabStates[TabIndex].QueuedCount,
		                         TabIndex == SelectedSubSectionIndex);
	}
}

void UW_TrainingMenu::UpdateQuickNavButtons(const TSet<ENomadicSubtype>& AvailableBuildings,
                                            const ENomadicSubtype ShownBuilding)
{
	for (UW_TrainingQuickNavButton* QuickNavButton : M_QuickNavButtons)
	{
		const ENomadicSubtype Building = QuickNavButton->GetBuilding();
		QuickNavButton->UpdateQuickNavButton(AvailableBuildings.Contains(Building), Building == ShownBuilding);
	}
}

void UW_TrainingMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	using namespace TrainingMenuWidgets;

	CollectWidgetsInPanel(M_TrainingItemPanel, M_TrainingItems);
	CollectWidgetsInPanel(M_SubSectionPanel, M_SubSectionTabs);
	CollectWidgetsInPanel(M_QuickNavPanel, M_QuickNavButtons);

	for (const UW_TrainingSubSectionTab* Tab : M_SubSectionTabs)
	{
		MakeWidgetFillHorizontalBox(Tab);
	}
}

void UW_TrainingMenu::InitTrainingMenu_SetupQuickNavButtons(UTrainingMenuManager* TrainingMenuManager)
{
	UTrainingLayoutSubsystem* TrainingLayoutSubsystem = UTrainingLayoutSubsystem::Get(this);
	if (not IsValid(TrainingLayoutSubsystem))
	{
		RTSFunctionLibrary::ReportError("No training layout subsystem for the training menu;"
			" the quick navigation buttons stay hidden."
			"\n See UW_TrainingMenu::InitTrainingMenu_SetupQuickNavButtons.");
		return;
	}

	const TArray<FTrainingQuickNavEntry>& QuickNavEntries = TrainingLayoutSubsystem->GetQuickNavEntries();
	if (QuickNavEntries.Num() > M_QuickNavButtons.Num())
	{
		RTSFunctionLibrary::ReportError("The training layout has more quick navigation buildings than the"
			" training menu has buttons."
			"\n Buildings: " + FString::FromInt(QuickNavEntries.Num()) +
			"\n Buttons: " + FString::FromInt(M_QuickNavButtons.Num()));
	}

	for (int32 ButtonIndex = 0; ButtonIndex < M_QuickNavButtons.Num(); ++ButtonIndex)
	{
		UW_TrainingQuickNavButton* QuickNavButton = M_QuickNavButtons[ButtonIndex];
		QuickNavButton->InitQuickNavButton(TrainingMenuManager);
		// Every button starts hidden; the manager shows the ones of buildings the player can use.
		QuickNavButton->SetVisibility(ESlateVisibility::Collapsed);
		if (not QuickNavEntries.IsValidIndex(ButtonIndex))
		{
			continue;
		}
		FSlateBrush IconBrush;
		const bool bHasIcon = TrainingLayoutSubsystem->GetQuickNavIconBrush(QuickNavEntries[ButtonIndex], IconBrush);
		QuickNavButton->SetupForBuilding(QuickNavEntries[ButtonIndex].Building, bHasIcon ? &IconBrush : nullptr);
	}
}
