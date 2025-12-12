// Copyright (C) Bas Blokzijl - All rights reserved.


#include "ExpansionRequirement.h"

#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UExpansionRequirement::UExpansionRequirement()
{
	SetRequirementType(ERTSRequirement::Requirement_Expansion);
}

bool UExpansionRequirement::CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem)
{
	if (M_ExpansionRequired.IsNone())
	{
		RTSFunctionLibrary::ReportError(
			"THe required expansion to setup the following expansion requirement is invalid: "
			+ GetName());
		return false;
	}
	const IBuildingExpansionOwner* ExpansionOwner = Cast<IBuildingExpansionOwner>(Trainer->GetOwner());
	if (not ExpansionOwner)
	{
		RTSFunctionLibrary::ReportError(
			"Could not check expansion requirement as the Trainer's owner is not a valid Expansion Owner: "
			+ GetName());
		return false;
	}
	return ExpansionOwner->HasBxpItemOfType(static_cast<EBuildingExpansionType>(M_ExpansionRequired.SubtypeValue));
}

FTrainingOption UExpansionRequirement::GetRequiredUnit() const
{
	return M_ExpansionRequired;
}

void UExpansionRequirement::InitExpansionRequirement(const FTrainingOption& UnitRequired)
{
	if(UnitRequired.IsNone())
	{
		RTSFunctionLibrary::ReportError(
			"THe required expansion to setup the following expansion requirement is invalid: "
			+ GetName());
		return;
	}
	M_ExpansionRequired = UnitRequired;
}
