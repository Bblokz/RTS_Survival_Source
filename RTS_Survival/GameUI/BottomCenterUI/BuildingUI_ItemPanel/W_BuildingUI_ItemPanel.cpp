// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_BuildingUI_ItemPanel.h"

#include "Components/Button.h"
#include "RTS_Survival/GameUI/Hotkey/W_HotKey.h"
#include "RTS_Survival/Subsystems/HotkeyProviderSubsystem/RTSHotkeyTypes.h"
#include "RTS_Survival/GameUI/BottomCenterUI/BottomCenterUIPanel/W_BottomCenterUI.h"
#include "RTS_Survival/GameUI/BuildingExpansion/W_ItemBuildingExpansion.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


void UW_BuildingUI_ItemPanel::ShowCancelBuilding() const
{
	if (WS_CreateCancelBuilding)
	{
		WS_CreateCancelBuilding->SetActiveWidgetIndex(BuildingUISettings.CancelBuildingSwitchIndex);
	}
}

void UW_BuildingUI_ItemPanel::ShowConstructBuilding() const
{
	if (WS_CreateCancelBuilding)
	{
		WS_CreateCancelBuilding->SetActiveWidgetIndex(BuildingUISettings.ConstructBuildingSwitchIndex);
	}
}

void UW_BuildingUI_ItemPanel::ShowConvertToVehicle() const
{
	if (WS_CreateCancelBuilding)
	{
		WS_CreateCancelBuilding->SetActiveWidgetIndex(BuildingUISettings.ConvertToVehicleSwitchIndex);
	}
}

void UW_BuildingUI_ItemPanel::ShowCancelVehicleConversion() const
{
	if (WS_CreateCancelBuilding)
	{
		WS_CreateCancelBuilding->SetActiveWidgetIndex(BuildingUISettings.CancelVehicleConversionSwitchIndex);
	}
}

void UW_BuildingUI_ItemPanel::DetermineNomadicButton(const EActionUINomadicButton NomadicButtonState,
                                                     ENomadicSubtype NomadicSubtype)
{
	// Init the correct convert to building and convert to truck button brushes using the data table row of the type.
	BP_SetNomadicButtonStyleForType(NomadicSubtype);
	M_LastBuildingButtonState = NomadicButtonState;
	switch (NomadicButtonState)
	{
	case EActionUINomadicButton::EAUI_ShowConvertToBuilding:
		ShowConstructBuilding();
		break;
	case EActionUINomadicButton::EAUI_ShowCancelBuilding:
		ShowCancelBuilding();
		break;
	case EActionUINomadicButton::EAUI_ShowConvertToVehicle:
		ShowConvertToVehicle();
		break;
	case EActionUINomadicButton::EAUI_ShowCancelVehicleConversion:
		ShowCancelVehicleConversion();
		break;
	}
}

void UW_BuildingUI_ItemPanel::SimulateClickNomadicExpandButton()
{
	switch (M_LastBuildingButtonState) {
	case EActionUINomadicButton::EAUI_ShowConvertToBuilding:
		OnClickedCreateBuildingButton();
		break;
	case EActionUINomadicButton::EAUI_ShowCancelBuilding:
		OnClickedCancelBuildingButton();
		break;
	case EActionUINomadicButton::EAUI_ShowConvertToVehicle:
		OnClickedCancelVehicleConversionButton();
		break;
	case EActionUINomadicButton::EAUI_ShowCancelVehicleConversion:
		OnClickedCancelVehicleConversionButton();
		break;
	}	
}

void UW_BuildingUI_ItemPanel::SimulateClickBuildingExpansion(const int32 Index)
{
	if (not M_BxpItems.IsValidIndex(Index))
	{
		RTSFunctionLibrary::ReportError("Invalid bxp item index through simulation (chorded action triggered on player controller"
								  "index provided: "
		  "\n  " +  FString::FromInt(Index)
		  + "\n while amount bxp items: " + FString::FromInt(M_BxpItems.Num()));
		return;
	}
	UW_ItemBuildingExpansion* Item =  M_BxpItems[Index];
	if (not IsValid(Item))
	{
		return;
	}
	Item->SimulatedClickedBxpItem();
}

bool UW_BuildingUI_ItemPanel::TryActivateBuildingExpansionFromHotkey(const int32 Index) const
{
	// Hidden while no building (or nomadic vehicle) is the primary selection: the items then hold stale data.
	if (not IsVisible())
	{
		return false;
	}

	// More hotkeys than item slots is fine: the extra hotkeys wait for a wider panel.
	if (not M_BxpItems.IsValidIndex(Index) || not IsValid(M_BxpItems[Index]))
	{
		return false;
	}

	return M_BxpItems[Index]->TryActivateFromHotkey();
}

TArray<UW_ItemBuildingExpansion*> UW_BuildingUI_ItemPanel::GetBxpItems() const
{
	if (M_BxpItems.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(
			"UW_BuildingUI_ItemPanel::GetBxpItems was called before the panel was initialized.");
	}
	return M_BxpItems;
}


void UW_BuildingUI_ItemPanel::InitArray(TArray<UW_ItemBuildingExpansion*> Items)
{
	M_BxpItems = Items;
}

void UW_BuildingUI_ItemPanel::OnClickedCancelBuildingButton()
{
	if (not EnsureValidParent())
	{
		return;
	}
	M_ParentWidget->CancelBuilding();
}

void UW_BuildingUI_ItemPanel::OnClickedCreateBuildingButton()
{
	if (not EnsureValidParent())
	{
		return;
	}
	M_ParentWidget->ConstructBuilding();
}

void UW_BuildingUI_ItemPanel::OnClickedConvertToVehicleButton()
{
	if (not EnsureValidParent())
	{
		return;
	}
	M_ParentWidget->ConvertToVehicle();
}

void UW_BuildingUI_ItemPanel::OnClickedCancelVehicleConversionButton()
{
	if (not EnsureValidParent())
	{
		return;
	}
	M_ParentWidget->CancelVehicleConversion();
}

bool UW_BuildingUI_ItemPanel::GetIsValidCreateBuildingButton() const
{
	if (CreateBuildingButton) { return true; }
	RTSFunctionLibrary::ReportError("CreateBuildingButton is null in UW_BuildingUI_ItemPanel during initialization.");
	return false;
}

bool UW_BuildingUI_ItemPanel::GetIsValidCancelBuildingButton() const
{
	if (CancelBuildingButton) { return true; }
	RTSFunctionLibrary::ReportError("CancelBuildingButton is null in UW_BuildingUI_ItemPanel during initialization.");
	return false;
}

bool UW_BuildingUI_ItemPanel::GetIsValidConvertToVehicleButton() const
{
	if (ConvertToVehicleButton) { return true; }
	RTSFunctionLibrary::ReportError("ConvertToVehicleButton is null in UW_BuildingUI_ItemPanel during initialization.");
	return false;
}

bool UW_BuildingUI_ItemPanel::GetIsValidCancelVehicleConversionButton() const
{
	if (CancelVehicleConversionButton) { return true; }
	RTSFunctionLibrary::ReportError(
		"CancelVehicleConversionButton is null in UW_BuildingUI_ItemPanel during initialization.");
	return false;
}




void UW_BuildingUI_ItemPanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (not GetIsValidNomadicExpansionHotkey())
	{
		return;
	}

	// Same badge rows as the bxp items next to it.
	M_Hotkey_NomadicExpansion->ShowChordedAction(
		RTSHotkeyTypes::NomadicExpansionActionName,
		TEXT("Text_BxpHotkeyMod"),
		TEXT("Text_BxpHotkeyKey"));
}

bool UW_BuildingUI_ItemPanel::GetIsValidNomadicExpansionHotkey() const
{
	if (IsValid(M_Hotkey_NomadicExpansion))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_Hotkey_NomadicExpansion"),
		TEXT("UW_BuildingUI_ItemPanel::GetIsValidNomadicExpansionHotkey"),
		this
	);
	return false;
}
