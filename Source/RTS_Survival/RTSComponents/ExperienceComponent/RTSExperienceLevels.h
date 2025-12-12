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

UENUM()
enum class EExperiencePerkType : uint8
{
	EP_None,
	EP_Health,
	EP_Damage,
	EP_Accuracy,
	EP_Speed,
	EP_SightRange,
	EP_Range,
	EP_ReloadSpeed,
};

USTRUCT()
struct FExperiencePerk 
{
	GENERATED_BODY()

    FExperiencePerk() 
        : PerkType(EExperiencePerkType::EP_None)
        , PerkValue(0.f)
    {
    }

    FExperiencePerk(EExperiencePerkType Type, float Value)
        : PerkType(Type)
        , PerkValue(Value)
    {
    }


	UPROPERTY()
	EExperiencePerkType PerkType = EExperiencePerkType::EP_None;

	UPROPERTY()
	float PerkValue = 0.f;
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
    FExperienceLevel(const int32 ExperienceNeeded, const TArray<FExperiencePerk>& InPerks)
        : CumulativeExperienceNeeded(ExperienceNeeded)
        , Perks(InPerks)
    {
    }

	UPROPERTY()
	int32 CumulativeExperienceNeeded = 0.f;

	UPROPERTY()
	TArray<FExperiencePerk> Perks = {};
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
