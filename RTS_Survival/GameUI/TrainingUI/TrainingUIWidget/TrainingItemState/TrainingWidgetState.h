#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"

#include "TrainingWidgetState.generated.h"


enum class ETechnology : uint8;

UENUM(Blueprintable)
enum class ETrainingItemStatus : uint8
{
	TS_LockedByTech,
	TS_LockedByUnit,
	TS_LockedByNeedVacantAircraftPad,
	TS_LockedByExpansion,
	TS_Unlocked
};

UENUM()
enum class ERequirementCount : uint8
{
	SingleRequirement,
	DoubleAndRequirement,
	DoubleOrRequirement
};



USTRUCT(BlueprintType, Blueprintable)
struct FTrainingWidgetState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	FTrainingOption ItemID = FTrainingOption();
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	int TotalTrainingTime = 0;
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	ETrainingItemStatus TrainingStatus = ETrainingItemStatus::TS_Unlocked;
	// How many of this item are in the queue.
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	int Count = 0;
	UPROPERTY(BlueprintReadOnly)
	ERequirementCount RequirementCount;
	UPROPERTY(BlueprintReadOnly)
	FTrainingOption RequiredUnit;
	UPROPERTY(BlueprintReadOnly)
	ETechnology RequiredTech = ETechnology::Tech_NONE;
	// Only used when a requirement has a second condition.
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	ETrainingItemStatus TrainingStatusSecondRequirement = ETrainingItemStatus::TS_Unlocked;
	UPROPERTY(BlueprintReadOnly)
	FTrainingOption SecondaryRequiredUnit;
	// Only used when a requirement has a second condition.
	UPROPERTY(BlueprintReadOnly)
	ETechnology SecondaryRequiredTech = ETechnology::Tech_NONE;
	// Only used when a requirement has a second condition, on a single requirement we let the TrainingStatus drive this.
	UPROPERTY(BlueprintReadOnly)
	bool bIsFirstRequirementMet = false;
	// Only used when a requirement has a second condition.
	UPROPERTY(BlueprintReadOnly)
	bool bIsSecondRequirementMet = false;
};
