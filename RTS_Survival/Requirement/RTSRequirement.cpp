// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "RTSRequirement.h"

#include "RTS_Survival/GameUI/TrainingUI/TrainingUIWidget/TrainingItemState/TrainingWidgetState.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

URTSRequirement::URTSRequirement()
{
	M_RequirementType = ERTSRequirement::Requirement_None;
	M_RequirementCount = ERequirementCount::SingleRequirement;
}

bool URTSRequirement::CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem)
{
	return false;
}

FTrainingOption URTSRequirement::GetRequiredUnit() const
{
	return FTrainingOption();
}

ETrainingItemStatus URTSRequirement::GetSecondaryRequirementStatus() const
{
	return ETrainingItemStatus::TS_Unlocked;
}

ETrainingItemStatus URTSRequirement::GetTrainingStatusWhenRequirementNotMet() const
{
	switch (M_RequirementType)
	{
	case ERTSRequirement::Requirement_None:
		RTSFunctionLibrary::ReportError("The type of requirement is not set; will allow option to be unlocked."
			"\n RequirementUObject: " + GetName());
		return ETrainingItemStatus::TS_Unlocked;
	case ERTSRequirement::Requirement_Tech:
		return ETrainingItemStatus::TS_LockedByTech;
	case ERTSRequirement::Requirement_Unit:
		return ETrainingItemStatus::TS_LockedByUnit;
	case ERTSRequirement::Requirement_VacantAircraftPad:
		return ETrainingItemStatus::TS_LockedByNeedVacantAircraftPad;
	case ERTSRequirement::Requirement_Expansion:
		return ETrainingItemStatus::TS_LockedByExpansion;
	}
	return ETrainingItemStatus::TS_Unlocked;
}
