// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnitCost.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/RTSExperienceLevels.h"
#include "SquadData.generated.h"


USTRUCT(blueprintable)
struct FSquadData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float MaxWalkSpeedCms = 0.0f;

	// Base is 600.
	UPROPERTY(BlueprintReadOnly)
	float MaxAcceleration = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float MaxHealth = 0.0f;
	
	UPROPERTY(BlueprintReadOnly)
	FResistanceAndDamageReductionData ResistancesAndDamageMlt;

	UPROPERTY(BlueprintReadOnly)
	float VisionRadius = 0.0f;

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
