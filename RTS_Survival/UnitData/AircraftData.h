#pragma once
#include "UnitCost.h"
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "AircraftData.generated.h"

struct FExperienceLevel;
enum class EAbilityID : uint8;

USTRUCT(BlueprintType)
struct FAircraftData
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	float VisionRadius = 1000.f;
	
	UPROPERTY(BlueprintReadOnly)
	float MaxHealth = 500.f;

	UPROPERTY(BlueprintReadOnly)
	FResistanceAndDamageReductionData ResistancesAndDamageMlt;
	
	UPROPERTY(BlueprintReadOnly)
	FUnitCost Cost;

        UPROPERTY(BlueprintReadOnly)
        TArray<FUnitAbilityEntry> Abilities;
	
	UPROPERTY()
	float ExperienceWorth = 0.0f;

	UPROPERTY()
	float ExperienceMultiplier = 0.0f;

	UPROPERTY()
	TArray<FExperienceLevel> ExperienceLevels;
};