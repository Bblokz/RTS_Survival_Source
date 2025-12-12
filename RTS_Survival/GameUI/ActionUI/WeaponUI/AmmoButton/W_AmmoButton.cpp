// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_AmmoButton.h"

#include "W_AmmoPicker.h"

void UW_AmmoButton::InitAmmoButton(
	const EWeaponShellType ShellType,
	UW_AmmoPicker* AmmoPicker)
{
	M_ShellType = ShellType;
	OnInitAmmoButton(ShellType);
	M_AmmoPicker = AmmoPicker;
}

void UW_AmmoButton::OnAmmoButtonClicked()
{
	if(IsValid(M_AmmoPicker))
	{
		M_AmmoPicker->OnShellTypeSelected(M_ShellType);
	}
}
