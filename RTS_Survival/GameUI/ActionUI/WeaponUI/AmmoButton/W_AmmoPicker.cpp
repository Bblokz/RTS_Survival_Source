// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_AmmoPicker.h"

#include "W_AmmoButton.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/GameUI/ActionUI/WeaponUI/W_WeaponItem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


void UW_AmmoPicker::OnClickedWeaponItemToAmmoPick(
	UW_WeaponItem* WeaponItem,
	const TArray<EWeaponShellType>& ShellTypesToPickFrom)
{
	if (not IsValid(WeaponItem) || ShellTypesToPickFrom.Num() <= 1)
	{
		return;
	}
	if (not GetIsValidActionUIManager())
	{
		return;
	}

	M_ActionUIManager->OnClickedWeaponItemToAmmoPick();
	SetAmmoPickerVisibility(true);
	M_ActiveWeaponItem = WeaponItem;
	for (const auto EachAmmoBtn : M_AmmoButtons)
	{
		if (not IsValid(EachAmmoBtn))
		{
			continue;
		}

		EachAmmoBtn->SetVisibility(
			ShellTypesToPickFrom.Contains(EachAmmoBtn->GetShellType())
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
}

void UW_AmmoPicker::InitActionUIMngr(UActionUIManager* ActionUIManager)
{
	M_ActionUIManager = ActionUIManager;
}

void UW_AmmoPicker::InitAmmoPicker(const TArray<UW_AmmoButton*>& AmmoButtons)
{
	M_AmmoButtons = AmmoButtons;
	TArray<EWeaponShellType> ShellTypes = {
		EWeaponShellType::Shell_AP, EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_APHEBC,
		EWeaponShellType::Shell_HE, EWeaponShellType::Shell_APCR, EWeaponShellType::Shell_HEAT
	};
	if (ShellTypes.Num() > M_AmmoButtons.Num())
	{
		RTSFunctionLibrary::ReportError("The amount of ammo types is greater than the amount of ammo buttons, "
			"cannot init picker for all ammo types."
			"@ function: UW_AmmoPicker::InitAmmoPicker");
		return;
	}
	for (int32 ShellTypeIndex = 0; ShellTypeIndex < ShellTypes.Num(); ++ShellTypeIndex)
	{
		if (not IsValid(M_AmmoButtons[ShellTypeIndex]))
		{
			RTSFunctionLibrary::ReportNullErrorComponent(
				this,
				"M_AmmoButtons[ShellTypeIndex]",
				"InitAmmoPicker");
			continue;
		}

		M_AmmoButtons[ShellTypeIndex]->InitAmmoButton(ShellTypes[ShellTypeIndex], this);
	}
}

bool UW_AmmoPicker::GetIsValidActionUIManager() const
{
	if (M_ActionUIManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ActionUIManager"),
		TEXT("UW_AmmoPicker::GetIsValidActionUIManager"),
		this);
	return false;
}

bool UW_AmmoPicker::GetIsValidActiveWeaponItem() const
{
	if (M_ActiveWeaponItem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ActiveWeaponItem"),
		TEXT("UW_AmmoPicker::GetIsValidActiveWeaponItem"),
		this);
	return false;
}

void UW_AmmoPicker::OnShellTypeSelected(const EWeaponShellType SelectedShellType)
{
	SetAmmoPickerVisibility(false);
	if (not GetIsValidActiveWeaponItem() || not GetIsValidActionUIManager())
	{
		return;
	}
	M_ActiveWeaponItem->OnNewShellTypeSelected(SelectedShellType);
}

void UW_AmmoPicker::OnAmmoButtonHovered(const EWeaponShellType HoveredShellType, const bool bIsHovering) const
{
	if (not GetIsValidActionUIManager())
	{
		return;
	}
	M_ActionUIManager->OnShellTypeHovered(HoveredShellType, bIsHovering);
}

void UW_AmmoPicker::SetAmmoPickerVisibility(const bool bVisible)
{
	if (bVisible)
	{
		SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
}
