// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "TrainingOptionClassSetup.generated.h"

class AActor;

/** Blueprint-facing setup row for a nomadic subtype and its spawn class. */
USTRUCT(BlueprintType)
struct FNomadicTrainingOptionSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Nomadic")
	ENomadicSubtype NomadicSubtype = ENomadicSubtype::Nomadic_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Nomadic")
	TSoftClassPtr<AActor> UnitClass = nullptr;
};

/** Blueprint-facing setup row for a tank subtype and its spawn class. */
USTRUCT(BlueprintType)
struct FTankTrainingOptionSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Tank")
	ETankSubtype TankSubtype = ETankSubtype::Tank_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Tank")
	TSoftClassPtr<AActor> UnitClass = nullptr;
};

/** Blueprint-facing setup row for a squad subtype and its spawn class. */
USTRUCT(BlueprintType)
struct FSquadTrainingOptionSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Squad")
	ESquadSubtype SquadSubtype = ESquadSubtype::Squad_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Squad")
	TSoftClassPtr<AActor> UnitClass = nullptr;
};

/** Blueprint-facing setup row for an aircraft subtype and its spawn class. */
USTRUCT(BlueprintType)
struct FAircraftTrainingOptionSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Aircraft")
	EAircraftSubtype AircraftSubtype = EAircraftSubtype::Aircarft_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Aircraft")
	TSoftClassPtr<AActor> UnitClass = nullptr;
};
