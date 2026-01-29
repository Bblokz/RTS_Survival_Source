// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArmorAndResistanceData.h"
#include "UnitCost.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/RTSExperienceLevels.h"
#include "VehicleData.generated.h"

// Vehicle data used at begin play to load stats for the vehicle using the subtype enum.
// These stats are stored in cppGameState.
USTRUCT(Blueprintable)
struct FTankData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float TurretRotationSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float VehicleRotationSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float VehicleMaxSpeedKmh = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float VehicleReverseSpeedKmh = 0.0f;

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
	
	UPROPERTY(BlueprintReadOnly)
	int32 EnergyRequired = 0;
};
