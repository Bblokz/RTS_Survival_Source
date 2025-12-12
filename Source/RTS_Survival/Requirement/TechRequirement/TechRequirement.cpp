// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TechRequirement.h"

#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/Requirement/UnitRequirement/UnitRequirement.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

UTechRequirement::UTechRequirement()
{
	SetRequirementType(ERTSRequirement::Requirement_Tech);
	M_RequiredTechnology = ETechnology::Tech_NONE;
}

void UTechRequirement::InitTechRequirement(const ETechnology RequiredTechnology)
{
	if (RequiredTechnology != ETechnology::Tech_NONE)
	{
		M_RequiredTechnology = RequiredTechnology;
	}
	else
	{
		RTSFunctionLibrary::ReportError("Invalid tech provided to TechRequirement:"
			"\n Tech: " + Global_GetTechDisplayName(RequiredTechnology) +
			"\n RequirementUObject: " + GetName());
	}
}

bool UTechRequirement::CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem)
{
	if (IsValid(Trainer))
	{
		if (const UPlayerTechManager* PlayerTechManager = FRTS_Statics::GetPlayerTechManager(Trainer);
			IsValid(PlayerTechManager))
		{
			return PlayerTechManager->HasTechResearched(M_RequiredTechnology);
		}
		RTSFunctionLibrary::ReportError("No valid PlayerTechManager found to check tech requirement:"
			"\n Tech: " + Global_GetTechDisplayName(M_RequiredTechnology) +
			"\n RequirementUObject: " + GetName());
		return false;
	}
	RTSFunctionLibrary::ReportError("No valid world context object provided to check tech requirement:"
		"\n Tech: " + Global_GetTechDisplayName(M_RequiredTechnology) +
		"\n RequirementUObject: " + GetName());
	return false;
}


