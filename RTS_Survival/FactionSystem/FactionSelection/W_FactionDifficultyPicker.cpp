// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_FactionDifficultyPicker.h"

#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_FactionDifficultyPicker::SetFactionPlayerController(AFactionPlayerController* FactionPlayerController)
{
	M_FactionPlayerController = FactionPlayerController;
}

void UW_FactionDifficultyPicker::OnDifficultyChosen(
	const int32 DifficultyPercentage,
	const ERTSGameDifficulty SelectedDifficulty)
{
	if (not GetIsValidFactionPlayerController())
	{
		return;
	}

	M_FactionPlayerController->HandleFactionDifficultyChosen(DifficultyPercentage, SelectedDifficulty);
}

bool UW_FactionDifficultyPicker::GetIsValidFactionPlayerController() const
{
	if (M_FactionPlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_FactionPlayerController",
		"UW_FactionDifficultyPicker::GetIsValidFactionPlayerController",
		this
	);
	return false;
}
