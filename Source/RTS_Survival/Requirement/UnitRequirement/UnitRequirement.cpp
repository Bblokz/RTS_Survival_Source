// Copyright (C) Bas Blokzijl - All rights reserved.


#include "UnitRequirement.h"

#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

UUnitRequirement::UUnitRequirement()
{
	SetRequirementType(ERTSRequirement::Requirement_Unit);
}

bool UUnitRequirement::CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem)
{
	if (M_UnitRequired.IsNone())
	{
		RTSFunctionLibrary::ReportError(
			"The required unit to setup the following unit requirement is invalid: " + GetName());
		return false;
	}
	UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(Trainer);
	if (IsValid(GameUnitManager))
	{
		// todo hardcoded to only check requirments as if the player is player 1. For the CPU
		// this may need to be changed if the CPU will make use of the same training system.
		return RTSFunctionLibrary::RTSIsValid(GameUnitManager->FindUnitForPlayer(M_UnitRequired, 1));
	}
	RTSFunctionLibrary::ReportError("Could not check requirement as GameUnitManager is not valid"
		"\n RequirementUObject: " + GetName());
	return false;
}

FTrainingOption UUnitRequirement::GetRequiredUnit() const
{
	if (M_UnitRequired.IsNone())
	{
		RTSFunctionLibrary::ReportError(
			"The required unit to setup the following unit requirement is invalid: " + GetName()
			+ "\n will return an empty training option for GetRequiredUnit");
	}
	return M_UnitRequired;
}

void UUnitRequirement::InitUnitRequirement(const FTrainingOption& UnitRequired)
{
	if (UnitRequired.IsNone())
	{
		RTSFunctionLibrary::ReportError(
			"The required unit to setup the following unit requirement is invalid: " + GetName());
		return;
	}
	M_UnitRequired = UnitRequired;
}
