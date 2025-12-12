// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Requirement/RTSRequirement.h"
#include "TechRequirement.generated.h"



struct FTrainingOption;
class UUnitRequirement;
enum class ETechnology : uint8;
/**
 * @note Tech Requirements are created with the transient package, this means that they are not saved to disk. 
 */
UCLASS()
class RTS_SURVIVAL_API UTechRequirement : public URTSRequirement
{
	GENERATED_BODY()

public:
	/**
	 * 
	 */
	UTechRequirement();

	void InitTechRequirement(const ETechnology RequiredTechnology);
	/** @return The technology required overriden from the base where this returns the NONE tech. */
	virtual ETechnology GetRequiredTechnology() const override { return M_RequiredTechnology; }

	/** @return Whether the requirement is met */
	virtual bool CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem) override;

private:
	UPROPERTY()
	ETechnology M_RequiredTechnology;
};


