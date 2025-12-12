// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Requirement/RTSRequirement.h"
#include "VacantAirPadRequirement.generated.h"

class UAircraftOwnerComp;
/**
 * Checks whether the trainer has a vacant air pad to train the aircraft unit on.
 */
UCLASS()
class RTS_SURVIVAL_API UVacantAirPadRequirement : public URTSRequirement
{
	GENERATED_BODY()

public:
	UVacantAirPadRequirement();

	void InitVacantAirPadRequirement();
	
	/** @return Whether the requirement is met */
	virtual bool CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem) override;

private:
	UAircraftOwnerComp* GetValidAircraftOwnerComponent(UTrainerComponent* Trainer) const;

	FString M_DefaultRequirementText = "No vacant air pads available";
};
