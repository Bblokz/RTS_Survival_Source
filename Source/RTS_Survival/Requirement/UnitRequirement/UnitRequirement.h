// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Requirement/RTSRequirement.h"
#include "UnitRequirement.generated.h"

struct FTrainingOption;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UUnitRequirement : public URTSRequirement
{
	GENERATED_BODY()

public:
	UUnitRequirement();

	virtual bool CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem) override;
	virtual FTrainingOption GetRequiredUnit() const override;
	void InitUnitRequirement(const FTrainingOption& UnitRequired);

private:
	// Saves the unit type and subtype to identify the unit that is required.
	// NOTE: Must be UPROPERTY so DuplicateObject clones it in queue snapshots.
	UPROPERTY(VisibleAnywhere, Category="Requirement", meta=(AllowPrivateAccess="true"))
	FTrainingOption M_UnitRequired;
};
