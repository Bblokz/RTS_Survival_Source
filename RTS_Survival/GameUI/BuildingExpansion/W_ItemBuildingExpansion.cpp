// Copyright Bas Blokzijl - All rights reserved.
#include "W_ItemBuildingExpansion.h"

#include "Components/Button.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/Hotkey/W_HotKey.h"
#include "RTS_Survival/Subsystems/HotkeyProviderSubsystem/RTSHotkeyTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace BxpItemHotkeyLabel
{
	// Rows in DT_RichText sized for the 100 px bxp tiles.
	const TCHAR* ModifierStyle = TEXT("Text_BxpHotkeyMod");
	const TCHAR* KeyStyle = TEXT("Text_BxpHotkeyKey");
}

void UW_ItemBuildingExpansion::InitW_ItemBuildingExpansion(
	UMainGameUI* NewMainGameU,
	const int NewIndex)
{
	if (IsValid(NewMainGameU))
	{
		M_MainGameUI = NewMainGameU;
		M_WidgetScrollbarIndex = NewIndex;
		if (IsValid(M_BxpHotkey))
		{
			M_BxpHotkey->ShowChordedAction(
				RTSHotkeyTypes::GetBuildingExpansionActionName(NewIndex),
				BxpItemHotkeyLabel::ModifierStyle,
				BxpItemHotkeyLabel::KeyStyle);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("NewMainGameUI is not valid in InitW_ItemBuildingExpansion"
			"\n at function UW_ItemBuildingExpansion::InitW_ItemBuildingExpansion");
	}
}

void UW_ItemBuildingExpansion::UpdateItemBuildingExpansionData(
	const EBuildingExpansionType Type,
	const EBuildingExpansionStatus Status,
	const FBxpConstructionRules& ConstructionRules)
{
	M_StatusTypeAndRules.M_Type = Type;
	M_StatusTypeAndRules.M_Status = Status;
	M_StatusTypeAndRules.M_PackedExpansionConstructionRules = ConstructionRules;
	UpdateVisibleButtonState(Type, Status);
}

void UW_ItemBuildingExpansion::EnableDisableItem(const bool bEnabled)
{
	if (bEnabled)
	{
		OnItemEnabled(M_StatusTypeAndRules.M_Status, M_StatusTypeAndRules.M_Type);
	}
	else
	{
		OnItemDisabled(M_StatusTypeAndRules.M_Status);
	}
}

void UW_ItemBuildingExpansion::SimulatedClickedBxpItem()
{
	OnClickedItemBuildingExpansion();
}

bool UW_ItemBuildingExpansion::TryActivateFromHotkey()
{
	// Same gate as a mouse click: unused slots are collapsed and unavailable ones have their button disabled.
	if (not IsVisible() || not IsValid(M_BxpItemButton) || not M_BxpItemButton->GetIsEnabled())
	{
		return false;
	}

	OnClickedItemBuildingExpansion();
	return true;
}


void UW_ItemBuildingExpansion::OnClickedItemBuildingExpansion() const
{
	if (not EnsureMainGameUIIsValid())
	{
		return;
	}
	// Propagate to the main game UI.
	M_MainGameUI->ClickedItemBuildingExpansion(M_StatusTypeAndRules.M_Type,
	                                           M_StatusTypeAndRules.M_Status,
	                                           M_StatusTypeAndRules.M_PackedExpansionConstructionRules,
	                                           M_WidgetScrollbarIndex);
}

void UW_ItemBuildingExpansion::UpdateButtonWithGlobalSlateStyle() const
{
	if (not ButtonStyleAsset)
	{
		RTSFunctionLibrary::ReportError("ButtonStyle null."
			"\n at widget: " + GetName() +
			"\n Forgot to set style reference in UW_ItemBuildingExpansion::UpdateButtonWithGlobalSlateStyle?");

		return;
	}
	const FButtonStyle* ButtonStyle = ButtonStyleAsset->GetStyle<FButtonStyle>();
	if (ButtonStyle && M_BxpItemButton)
	{
		M_BxpItemButton->SetStyle(*ButtonStyle);
	}
}

bool UW_ItemBuildingExpansion::EnsureMainGameUIIsValid() const
{
	if (not IsValid(M_MainGameUI))
	{
		RTSFunctionLibrary::ReportError("MainGameUI is not valid in EnsureMainGameUIIsValid"
			"\n at function UW_ItemBuildingExpansion::EnsureMainGameUIIsValid");
		return false;
	}
	return true;
}
