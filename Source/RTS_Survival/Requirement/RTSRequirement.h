// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "UObject/Object.h"
#include "RTSRequirement.generated.h"


enum class ERequirementCount : uint8;
class UTrainerComponent;
struct FTrainingOption;
enum class ETechnology : uint8;
enum class ETrainingItemStatus : uint8;

UENUM()
enum class ERTSRequirement : uint8
{
	Requirement_None,
	Requirement_Tech,
	Requirement_Unit,
	// For specific expansion at the training component's owner.
	Requirement_Expansion,
	Requirement_VacantAircraftPad
};

/**
 * @note Requirements are created with the transient package and are not saved to disk 
 */
UCLASS()
class RTS_SURVIVAL_API URTSRequirement : public UObject
{
	GENERATED_BODY()

public:
	URTSRequirement();

	/** @return Whether the requirement is met */
	virtual bool CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem);
	// This is here so we do not need to cast to a specific requirement type to get the required technology.
	// Instead we can use the requirement type to determine what attribute to get.
	virtual ETechnology GetRequiredTechnology() const { return ETechnology::Tech_NONE; }
	virtual FTrainingOption GetRequiredUnit() const;
	ERequirementCount GetRequirementCount() const { return M_RequirementCount; }
	inline ERTSRequirement GetRequirementType() const { return M_RequirementType; }

	/** @brief Secondary status when not met (for composites). Singles default to Unlocked. */
	virtual ETrainingItemStatus GetSecondaryRequirementStatus() const;

	virtual ETrainingItemStatus GetTrainingStatusWhenRequirementNotMet() const;

protected:
	void SetRequirementCount(const ERequirementCount NewRequirementCount) { M_RequirementCount = NewRequirementCount; }
	inline void SetRequirementType(const ERTSRequirement NewRequirementType) { M_RequirementType = NewRequirementType; }

private:
	UPROPERTY()
	ERequirementCount M_RequirementCount;
	UPROPERTY()
	ERTSRequirement M_RequirementType;
};
