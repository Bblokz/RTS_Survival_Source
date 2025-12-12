// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_SelectedUnitsRow.h"

#include "RTS_Survival/GameUI/BottomCenterUI/SelectionUI/W_SelectedUnit/W_SelectedUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
void UW_SelectedUnitsRow::SetupRowForSelectedUnits(TArray<FSelectedUnitsWidgetState> WidgetStatesRow)
{
	for (int32 i = 0; i < WidgetStatesRow.Num(); ++i)
	{
		if (UW_SelectedUnit* Tile = GetIsValidSelectedUnitWidget_AtIndex(i))
		{
			// Make sure it becomes visible & hit-testable when reused on a new page.
			Tile->SetVisibility(ESlateVisibility::Visible);
			Tile->SetIsEnabled(true);

			ApplyStateToTile(Tile, WidgetStatesRow[i]);
		}
	}
	// Hide the rest.
	HideUnusedTilesFromIndex(WidgetStatesRow.Num());
}

void UW_SelectedUnitsRow::HideUnusedTilesFromIndex(const int32 VisibleCount)
{
	for (int32 i = VisibleCount; i < SelectedUnitsWidgets.Num(); ++i)
	{
		if (UW_SelectedUnit* Tile = SelectedUnitsWidgets[i])
		{
			// Hidden: not visible and not hit-testable; keeps the slot’s layout.
			// If you need the slot to collapse entirely, switch to Collapsed.
			Tile->SetVisibility(ESlateVisibility::Hidden);
			Tile->SetIsEnabled(false);
		}
	}
}


void UW_SelectedUnitsRow::InitTilesOwner(UW_SelectionPanel* OwnerPanel)
{
	for (UW_SelectedUnit* Tile : SelectedUnitsWidgets)
	{
		if (IsValid(Tile))
		{
			Tile->InitSelectionPanelOwner(OwnerPanel);
		}
	}
}

UW_SelectedUnit* UW_SelectedUnitsRow::GetIsValidSelectedUnitWidget_AtIndex(const int32 Index)
{
	if (not SelectedUnitsWidgets.IsValidIndex(Index))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(TEXT("Index %d is out of range in SelectedUnitsRow"), Index));
		return nullptr;
	}
	if (IsValid(SelectedUnitsWidgets[Index]))
	{
		return SelectedUnitsWidgets[Index];
	}
	RTSFunctionLibrary::ReportError(FString::Printf(TEXT("SelectedUnitsWidgets[%d] invalid."), Index));
	return nullptr;
}

void UW_SelectedUnitsRow::ApplyStateToTile(UW_SelectedUnit* const Tile, const FSelectedUnitsWidgetState& State) const
{
	// BP must call CacheSelectedUnitState after Setup... but we also expose it so call directly if you prefer:
	Tile->SetupSelectedUnitWidget(State);
}
