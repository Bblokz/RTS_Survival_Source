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
	if (!IsValid(WeaponItem) || ShellTypesToPickFrom.Num() <= 1)
	{
		return;
	}
	if (GetIsValidActionUIManager())
	{
		M_ActionUIManager->OnClickedWeaponItemToAmmoPick();
	}
	SetAmmoPickerVisibility(true);
	M_ActiveWeaponItem = WeaponItem;
	for (const auto EachAmmoBtn : M_AmmoButtons)
	{
		if (IsValid(EachAmmoBtn))
		{
			if (ShellTypesToPickFrom.Contains(EachAmmoBtn->GetShellType()))
			{
				EachAmmoBtn->SetVisibility(ESlateVisibility::Visible);
			}
			else
			{
				EachAmmoBtn->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
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
	if (ShellTypes.Num() > AmmoButtons.Num())
	{
		RTSFunctionLibrary::ReportError("The amount of ammo types is greater than the amount of ammo buttons, "
			"cannot init picker for all ammo types."
			"@ function: UW_AmmoPicker::InitAmmoPicker");
		return;
	}
	for (int i = 0; i < ShellTypes.Num(); i++)
	{
		if (IsValid(M_AmmoButtons[i]))
		{
			M_AmmoButtons[i]->InitAmmoButton(ShellTypes[i], this);
		}
		else
		{
			RTSFunctionLibrary::ReportNullErrorComponent(this,
			                                             "M_AmmoButtons[i]",
			                                             "InitAmmoPicker");
		}
	}
}

bool UW_AmmoPicker::GetIsValidActionUIManager() const
{
	if (M_ActionUIManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid action UI manager found, cannot set up ammo picker."
		"\n At function UW_AmmoPicker::GetIsValidActionUIManager");
	return false;
}

bool UW_AmmoPicker::GetIsValidActiveWeaponItem() const
{
	if (IsValid(M_ActiveWeaponItem))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid active weapon item found, cannot set up ammo picker."
		"\n At function UW_AmmoPicker::GetIsValidActiveWeaponItem");
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
	M_ActionUIManager->OnShellTypeSelected(SelectedShellType);
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
