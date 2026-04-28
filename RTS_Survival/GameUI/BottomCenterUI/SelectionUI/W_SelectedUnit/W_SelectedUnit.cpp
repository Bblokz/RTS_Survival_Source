// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_SelectedUnit.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "RTS_Survival/GameUI/BottomCenterUI/SelectionUI/W_SelectionPanel/W_SelectionPanel.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Slate/SlateBrushAsset.h"
#include "Input/Reply.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

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
	if (not GetIsValidSelectionPanelOwner())
	{
		return;
	}

	constexpr float DoubleClickThresholdSeconds = 0.30f;
	const UWorld* CurrentWorld = GetWorld();
	if (not IsValid(CurrentWorld))
	{
		M_SelectionPanelOwner.Get()->HandleSelectedUnitClicked(M_CurrentWidgetState);
		return;
	}

	const float CurrentTimeSeconds = CurrentWorld->GetTimeSeconds();
	const bool bIsDoubleClick =
		(CurrentTimeSeconds - M_LastSelectedUnitClickTimeSeconds) <= DoubleClickThresholdSeconds;
	M_LastSelectedUnitClickTimeSeconds = CurrentTimeSeconds;

	if (bIsDoubleClick)
	{
		M_SelectionPanelOwner.Get()->HandleSelectedUnitDoubleClicked(M_CurrentWidgetState);
		return;
	}

	M_SelectionPanelOwner.Get()->HandleSelectedUnitClicked(M_CurrentWidgetState);
}

FReply UW_SelectedUnit::NativeOnMouseButtonDoubleClick(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent
)
{
	const FReply ParentReply = Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return ParentReply;
	}
	if (not GetIsValidSelectionPanelOwner())
	{
		return ParentReply;
	}

	M_SelectionPanelOwner.Get()->HandleSelectedUnitDoubleClicked(M_CurrentWidgetState);
	return FReply::Handled();
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

bool UW_SelectedUnit::GetIsValidSelectionPanelOwner() const
{
	if (M_SelectionPanelOwner.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("UW_SelectedUnit: selection panel owner is invalid."));
	return false;
}
