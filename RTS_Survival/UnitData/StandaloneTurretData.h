// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArmorAndResistanceData.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"

#include "StandaloneTurretData.generated.h"

/** @brief Balance data loaded by a standalone turret from its configured unit subtype. */
USTRUCT(BlueprintType)
struct FStandaloneTurretData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float MaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float TurretRotationSpeedDegreesPerSecond = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FResistanceAndDamageReductionData ResistancesAndDamageMlt;

	UPROPERTY(BlueprintReadOnly)
	TArray<FUnitAbilityEntry> Abilities;
};
