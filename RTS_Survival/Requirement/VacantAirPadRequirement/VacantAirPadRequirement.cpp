// Copyright (C) Bas Blokzijl - All rights reserved.


#include "VacantAirPadRequirement.h"

#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AircraftOwnerComp/AircraftOwnerComp.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

UVacantAirPadRequirement::UVacantAirPadRequirement()
{
}

void UVacantAirPadRequirement::InitVacantAirPadRequirement()
{
	// The amount of slots is determined when we check the requirement.
	SetRequirementType(ERTSRequirement::Requirement_VacantAircraftPad);
}

bool UVacantAirPadRequirement::CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem)
{
	const UAircraftOwnerComp* AircraftOwnerComp = GetValidAircraftOwnerComponent(Trainer);
	if (not IsValid(AircraftOwnerComp))
	{
		return false;
	}
	// Special case: Aircraft pads can only be used by aircraft without one if they are vacant meaning there is no
	// training aircraft or created aircraft assigned to it. This means that even if no vacant pads are left;
	// queued aircraft already have a pad assigned and can continue training.
	if (bIsCheckingForQueuedItem)
	{
		return true;
	}
	// Note that a requirement text is only used if the requirement is not met, to inform the player.
	// So we can just provide the amount as air pads in use with this as if this is shown then all are in use.
	const int32 AirPadsInUse = AircraftOwnerComp->GetAmountAirPads();
	
	// If not full we have a vacant air pad.
	return not AircraftOwnerComp->IsAirBaseFull();
}

UAircraftOwnerComp* UVacantAirPadRequirement::GetValidAircraftOwnerComponent(UTrainerComponent* Trainer) const
{
	UAircraftOwnerComp* AircraftOwnerComp = nullptr;
	if (not IsValid(Trainer))
	{
		return nullptr;
	}
	AActor* Owner = Trainer->GetOwner();
	if (not IsValid(Owner))
	{
		RTSFunctionLibrary::ReportError("Failed to get valid owner from TrainerComponent in VacantAirPadRequirement"
			"\n TrainerComponent: " + Trainer->GetName() +
			"\n See VacantAirPadRequirement::GetValidAircraftOwnerComponent");
		return nullptr;
	}
	AircraftOwnerComp = Owner->FindComponentByClass<UAircraftOwnerComp>();
	if (not IsValid(AircraftOwnerComp))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to get valid AircraftOwnerComp from TrainerComponent owner in VacantAirPadRequirement"
			"\n TrainerComponent: " + Trainer->GetName() +
			"\n Owner: " + Owner->GetName() +
			"\n See VacantAirPadRequirement::GetValidAircraftOwnerComponent");
		return nullptr;
	}
	return AircraftOwnerComp;
}
