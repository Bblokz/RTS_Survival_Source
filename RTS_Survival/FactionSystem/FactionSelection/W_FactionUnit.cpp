// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_FactionUnit.h"

#include "Components/Button.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_FactionSelectionMenu.h"

void UW_FactionUnit::NativeConstruct()
{
	Super::NativeConstruct();

	if (not GetIsValidUnitButton())
	{
		return;
	}

	M_UnitButton->OnClicked.AddDynamic(this, &UW_FactionUnit::HandleUnitButtonClicked);
}

void UW_FactionUnit::SetTrainingOption(const FTrainingOption& TrainingOption)
{
	M_TrainingOption = TrainingOption;
	BP_OnTrainingOptionChanged(M_TrainingOption);
}

void UW_FactionUnit::SetFactionSelectionMenu(UW_FactionSelectionMenu* FactionSelectionMenu)
{
	M_FactionSelectionMenu = FactionSelectionMenu;
}

void UW_FactionUnit::SimulateUnitButtonClick()
{
	HandleUnitButtonClicked();
}

void UW_FactionUnit::HandleUnitButtonClicked()
{
	if (not GetIsValidFactionSelectionMenu())
	{
		return;
	}

	M_FactionSelectionMenu->HandleUnitSelected(M_TrainingOption);
}

bool UW_FactionUnit::GetIsValidUnitButton() const
{
	if (not IsValid(M_UnitButton))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_UnitButton",
			"GetIsValidUnitButton",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionUnit::GetIsValidFactionSelectionMenu() const
{
	if (not M_FactionSelectionMenu.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_FactionSelectionMenu",
			"GetIsValidFactionSelectionMenu",
			this
		);
		return false;
	}

	return true;
}
