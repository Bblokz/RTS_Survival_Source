// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_BuildingUI_ItemPanel.h"

#include "Components/Button.h"
#include "RTS_Survival/GameUI/BottomCenterUI/BottomCenterUIPanel/W_BottomCenterUI.h"
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



