// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Requirement/RTSRequirement.h"
#include "ExpansionRequirement.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UExpansionRequirement : public URTSRequirement
{
	GENERATED_BODY()
public:
	UExpansionRequirement();
	virtual bool CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem) override;
	virtual FTrainingOption GetRequiredUnit() const override;
	void InitExpansionRequirement(const FTrainingOption& UnitRequired);

private:

	// The required Expansion at the Trainer component's owner.
	// NOTE: Must be UPROPERTY so DuplicateObject clones it in queue snapshots.
	UPROPERTY(VisibleAnywhere, Category="Requirement", meta=(AllowPrivateAccess="true"))
	FTrainingOption M_ExpansionRequired;
};
