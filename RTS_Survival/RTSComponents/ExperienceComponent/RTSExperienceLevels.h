#pragma once

#include "CoreMinimal.h"

#include "RTSExperienceLevels.generated.h"

// Needs to match the string index of the data table that contains the icons exactly.
UENUM(BlueprintType)
enum class EVeterancyIconSet : uint8
{
	EVI_None,
	EVI_GerVehicles,
	EVI_GerInfantry,
	EVI_GerAircraft,
};

USTRUCT()
struct FExperienceLevel
{
	GENERATED_BODY()

	 FExperienceLevel() 
        : CumulativeExperienceNeeded(0.f)
    {
    }

    // Parameterized constructor for convenience.
    explicit FExperienceLevel(const int32 ExperienceNeeded)
        : CumulativeExperienceNeeded(ExperienceNeeded)
    {
    }

	UPROPERTY()
	int32 CumulativeExperienceNeeded = 0.f;

};

USTRUCT()
struct FUnitExperience
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FExperienceLevel> Levels = {};

	UPROPERTY()
	int32 M_ExperienceMultiplier = 1;

	UPROPERTY()
	int M_CurrentLevel = -1;

	UPROPERTY()
	int32 M_CumulativeExperience = 0;

	UPROPERTY()
	EVeterancyIconSet M_VeterancyIconSet = EVeterancyIconSet::EVI_None;
	
};
