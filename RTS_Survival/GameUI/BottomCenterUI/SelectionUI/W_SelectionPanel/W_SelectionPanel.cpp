// Copyright (C) Bas Blokzijl - All rights.

#include "W_SelectionPanel.h"

#include "RTS_Survival/GameUI/BottomCenterUI/SelectionUI/W_EnumerateUnitSelection/W_EnumerateUnitSelection.h"
#include "RTS_Survival/GameUI/BottomCenterUI/SelectionUI/W_SelectedUnitsRow/W_SelectedUnitsRow.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_SelectionPanel::RebuildSelectionUI(const TArray<FSelectedUnitsWidgetState>& AllWidgetStates,
                                           const int32 ActivePageIndex)
{
	// Debug(AllWidgetStates, ActivePageIndex);
	if (not EnsureSelectedUnitsRowsAreValid())
	{
		return;
	}

	M_CachedFlatStates = AllWidgetStates;

	const int32 Row1Cap = SelectedUnitsRow1->GetCapacity();
	const int32 Row2Cap = SelectedUnitsRow2->GetCapacity();
	const int32 UnitsPerPage = Row1Cap + Row2Cap;

	if (UnitsPerPage <= 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_SelectionPanel: UnitsPerPage == 0; check row capacities."));
		return;
	}

	const int32 TotalUnits = M_CachedFlatStates.Num();
	const int32 PageCount = FMath::Max(1, (TotalUnits + UnitsPerPage - 1) / UnitsPerPage);

	M_CurrentPage = FMath::Clamp(ActivePageIndex, 0, PageCount - 1);

	UpdateEnumerateButtonsVisibilityAndLabels(PageCount, M_CurrentPage);
	DrawPage(M_CachedFlatStates, M_CurrentPage);
}

void UW_SelectionPanel::InitMainGameUI(ACPPController* PlayerController)
{
	M_PlayerController = PlayerController;
}

void UW_SelectionPanel::InitBeforeMainGameUIInit()
{
	// Validate rows first (early returns).
	if (not GetIsValidSelectedUnitsRow1())
	{
		return;
	}

	if (not GetIsValidSelectedUnitsRow2())
	{
		return;
	}

	SelectedUnitsRow1->InitArraysBeforeMainGameUIInit();
	SelectedUnitsRow2->InitArraysBeforeMainGameUIInit();
	SelectedUnitsRow1->InitTilesOwner(this);
	SelectedUnitsRow2->InitTilesOwner(this);

	InitializeEnumerateButtonsFromRows();
}

UW_EnumerateUnitSelection* UW_SelectionPanel::GetEnumerateButtonAt(const int32 Index) const
{
	if (not bM_EnumerateButtonsInitialized)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UW_SelectionPanel::GetEnumerateButtonAt called before buttons were initialized."));
		return nullptr;
	}

	if (Index < 0 || Index > 3)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UW_SelectionPanel::GetEnumerateButtonAt index out of range. Expected [0..3]."));
		return nullptr;
	}

	return M_EnumerateButtons[Index];
}

void UW_SelectionPanel::HandleSelectedUnitClicked(const FSelectedUnitsWidgetState& ClickedState)
{
	if (not GetIsValidPlayerController())
	{
		return;
	}

	const bool bShift = M_PlayerController->GetIsShiftPressed();
	const bool bCtrl = M_PlayerController->GetIsControlPressed();

	if (bCtrl)
	{
		// Do not mutate arrays except to filter to this type.
		M_PlayerController->OnlySelectUnitsOfType(ClickedState.UnitID);
		return;
	}

	if (bShift)
	{
		// Remove a single unit instance at a specific index (controller verifies existence).
		M_PlayerController->RemoveUnitAtIndex(ClickedState.UnitID, ClickedState.SelectionArrayIndex);
		return;
	}

	// Bare click: do not change arrays; ask to set the primary selection type if the controller supports it.
	if (M_PlayerController->TryAdvancePrimaryToUnitType(ClickedState.UnitID, ClickedState.SelectionArrayIndex))
	{
		return;
	}

	// Fallback (no-op): helpful debug note.
	RTSFunctionLibrary::PrintString(
		TEXT("UW_SelectionPanel: tile clicked; no primary-type setter available."), FColor::Cyan);
}

void UW_SelectionPanel::HandleEnumerateClicked(const int32 NewPageIndex)
{
	if (M_CachedFlatStates.Num() == 0)
	{
		return;
	}
	if (not EnsureSelectedUnitsRowsAreValid())
	{
		return;
	}

	const int32 Row1Cap = SelectedUnitsRow1->GetCapacity();
	const int32 Row2Cap = SelectedUnitsRow2->GetCapacity();
	const int32 UnitsPerPage = Row1Cap + Row2Cap;
	if (UnitsPerPage <= 0)
	{
		return;
	}

	const int32 TotalUnits = M_CachedFlatStates.Num();
	const int32 PageCount = FMath::Max(1, (TotalUnits + UnitsPerPage - 1) / UnitsPerPage);

	M_CurrentPage = FMath::Clamp(NewPageIndex, 0, PageCount - 1);

	UpdateEnumerateButtonsVisibilityAndLabels(PageCount, M_CurrentPage);
	DrawPage(M_CachedFlatStates, M_CurrentPage);
}

void UW_SelectionPanel::InitializeEnumerateButtonsFromRows()
{
	// Pull from the two rows.
	UW_EnumerateUnitSelection* LocalButtons[4] = {
		SelectedUnitsRow1 ? SelectedUnitsRow1->GetEnumerateButton1() : nullptr,
		SelectedUnitsRow1 ? SelectedUnitsRow1->GetEnumerateButton2() : nullptr,
		SelectedUnitsRow2 ? SelectedUnitsRow2->GetEnumerateButton1() : nullptr,
		SelectedUnitsRow2 ? SelectedUnitsRow2->GetEnumerateButton2() : nullptr
	};

	// Validate each; keep nesting shallow via early returns.
	if (not GetIsValidEnumerateButton(LocalButtons[0], TEXT("Row1.EnumerateButton1")))
	{
		return;
	}

	if (not GetIsValidEnumerateButton(LocalButtons[1], TEXT("Row1.EnumerateButton2")))
	{
		return;
	}

	if (not GetIsValidEnumerateButton(LocalButtons[2], TEXT("Row2.EnumerateButton1")))
	{
		return;
	}

	if (not GetIsValidEnumerateButton(LocalButtons[3], TEXT("Row2.EnumerateButton2")))
	{
		return;
	}

	// Commit to member array (UPROPERTY so GC tracks these references).
	M_EnumerateButtons[0] = LocalButtons[0];
	M_EnumerateButtons[1] = LocalButtons[1];
	M_EnumerateButtons[2] = LocalButtons[2];
	M_EnumerateButtons[3] = LocalButtons[3];

	bM_EnumerateButtonsInitialized = true;
}

void UW_SelectionPanel::UpdateEnumerateButtonsVisibilityAndLabels(const int32 PageCount, const int32 ActivePage)
{
	if (not bM_EnumerateButtonsInitialized)
	{
		InitializeEnumerateButtonsFromRows();
		if (not bM_EnumerateButtonsInitialized)
		{
			return;
		}
	}

	// We have 4 buttons total; hide unused; label from 1..PageCount
	for (int32 i = 0; i < 4; ++i)
	{
		if (UW_EnumerateUnitSelection* Btn = M_EnumerateButtons[i])
		{
			if (i < PageCount)
			{
				Btn->SetVisibility(ESlateVisibility::Visible);
				Btn->InitOwnerAndIndex(this, i);
				Btn->SetLabel(FText::AsNumber(i + 1));
				// Visual highlight for ActivePage can be handled in BP using the label/page index.
			}
			else
			{
				Btn->SetVisibility(ESlateVisibility::Hidden);
			}
		}
	}
}

void UW_SelectionPanel::DrawPage(const TArray<FSelectedUnitsWidgetState>& AllWidgetStates, const int32 PageIndex)
{
	if (not EnsureSelectedUnitsRowsAreValid())
	{
		return;
	}

	const int32 Row1Cap = SelectedUnitsRow1->GetCapacity();
	const int32 Row2Cap = SelectedUnitsRow2->GetCapacity();
	const int32 UnitsPerPage = Row1Cap + Row2Cap;

	const int32 Start = PageIndex * UnitsPerPage;
	if (Start >= AllWidgetStates.Num())
	{
		// Nothing to draw; hide all tiles.
		SelectedUnitsRow1->HideUnusedTilesFromIndex(0);
		SelectedUnitsRow2->HideUnusedTilesFromIndex(0);
		return;
	}

	// Slice for row1
	const int32 CountRow1 = FMath::Min(Row1Cap, AllWidgetStates.Num() - Start);
	TArray<FSelectedUnitsWidgetState> Row1States;
	Row1States.Reserve(CountRow1);
	for (int32 i = 0; i < CountRow1; ++i)
	{
		Row1States.Add(AllWidgetStates[Start + i]);
	}
	SelectedUnitsRow1->SetupRowForSelectedUnits(Row1States);

	// Slice for row2
	const int32 StartRow2 = Start + CountRow1;
	const int32 CountRow2 = FMath::Clamp(AllWidgetStates.Num() - StartRow2, 0, Row2Cap);
	TArray<FSelectedUnitsWidgetState> Row2States;
	Row2States.Reserve(CountRow2);
	for (int32 i = 0; i < CountRow2; ++i)
	{
		Row2States.Add(AllWidgetStates[StartRow2 + i]);
	}
	SelectedUnitsRow2->SetupRowForSelectedUnits(Row2States);
}

bool UW_SelectionPanel::GetIsValidSelectedUnitsRow1() const
{
	if (IsValid(SelectedUnitsRow1))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("UW_SelectionPanel: SelectedUnitsRow1 is not bound or invalid."));
	return false;
}

bool UW_SelectionPanel::GetIsValidSelectedUnitsRow2() const
{
	if (IsValid(SelectedUnitsRow2))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("UW_SelectionPanel: SelectedUnitsRow2 is not bound or invalid."));
	return false;
}

bool UW_SelectionPanel::GetIsValidEnumerateButton(const UW_EnumerateUnitSelection* InPtr, const TCHAR* DebugName) const
{
	if (IsValid(InPtr))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		FString::Printf(TEXT("UW_SelectionPanel: Enumerate button '%s' is null or invalid."), DebugName));
	return false;
}

bool UW_SelectionPanel::EnsureSelectedUnitsRowsAreValid() const
{
	const bool bRow1 = GetIsValidSelectedUnitsRow1();
	const bool bRow2 = GetIsValidSelectedUnitsRow2();
	return bRow1 && bRow2;
}

bool UW_SelectionPanel::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("UW_SelectionPanel: player controller not set/valid."));
	return false;
}

void UW_SelectionPanel::Debug(const TArray<FSelectedUnitsWidgetState>& AllWidgetStates, int32 ActivePageIndex)
{
	// ---------- DEBUG: show how many widgets we’ll draw, grouped by type ----------
	{
		const int32 Total = AllWidgetStates.Num();

		TMap<FString, int32> TypeCounts;
		TypeCounts.Reserve(Total);

		for (const FSelectedUnitsWidgetState& State : AllWidgetStates)
		{
			// FTrainingOption::GetDisplayName() assumed to return FText
			const FString NiceName = State.UnitID.GetDisplayName();

			if (int32* Existing = TypeCounts.Find(NiceName))
			{
				++(*Existing);
			}
			else
			{
				TypeCounts.Add(NiceName, 1);
			}
		}

		FString Debug;
		Debug.Reserve(256);
		Debug += TEXT("UW_SelectionPanel::RebuildSelectionUI\n");
		Debug += FString::Printf(TEXT("Total widgets: %d\n"), Total);
		Debug += TEXT("Type breakdown:\n");
		for (const TPair<FString, int32>& Kvp : TypeCounts)
		{
			Debug += FString::Printf(TEXT("- %s: %d\n"), *Kvp.Key, Kvp.Value);
		}

		RTSFunctionLibrary::PrintString(Debug, FColor::Blue, 20);
	}
	// --------------------------------------------------------------------------	
}
