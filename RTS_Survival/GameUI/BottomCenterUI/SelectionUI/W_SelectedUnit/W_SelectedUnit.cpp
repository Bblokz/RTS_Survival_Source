// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_SelectedUnit.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "RTS_Survival/GameUI/BottomCenterUI/SelectionUI/W_SelectionPanel/W_SelectionPanel.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Slate/SlateBrushAsset.h"

void UW_SelectedUnit::SetupSelectedUnitWidget(const FSelectedUnitsWidgetState& WidgetState)
{
	CacheSelectedUnitState(WidgetState);
	BP_SetupSelectedUnitWidget(WidgetState);
	if (not IsValid(SelectedUnitButton))
	{
		return;
	}
	switch (WidgetState.WidgetType)
	{
	case ESelectedWidgetType::NotPrimary:
		SelectedUnitButton->SetStyle(M_NotPrimary);
		break;
	case ESelectedWidgetType::PrimarySameType:
		SelectedUnitButton->SetStyle(M_PrimaryTypeBtnStyle);
		break;
	case ESelectedWidgetType::PrimarySelected:
		SelectedUnitButton->SetStyle(M_PrimaryUnitBtnStyle);
		break;
	}
}

void UW_SelectedUnit::InitSelectionPanelOwner(UW_SelectionPanel* InOwner)
{
	if (not IsValid(InOwner))
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_SelectedUnit::InitSelectionPanelOwner called with null owner."));
		return;
	}
	M_SelectionPanelOwner = InOwner;
}

void UW_SelectedUnit::OnClickedSelectedUnit()
{
	if (not M_SelectionPanelOwner.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_SelectedUnit click: owner panel invalid."));
		return;
	}
	// Pass the cached state upward.
	M_SelectionPanelOwner.Get()->HandleSelectedUnitClicked(M_CurrentWidgetState);
}

void UW_SelectedUnit::CacheSelectedUnitState(const FSelectedUnitsWidgetState& NewState)
{
	M_CurrentWidgetState = NewState;
}

void UW_SelectedUnit::SetImage(USlateBrushAsset* ImageBrushAsset)
{
	if(not IsValid(SelectedUnitImage) || not IsValid(ImageBrushAsset))
	{
		return;
	}
	auto CurrBrush = SelectedUnitImage->GetBrush();
	CurrBrush.SetResourceObject(ImageBrushAsset);
}
