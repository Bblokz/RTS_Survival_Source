// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"
#include "WorldMissionEnemyArmy.generated.h"

/**
 * @brief The units of the world division a campaign operation is fought against, as the division has them now
 * (losses already removed). The world map fills it at launch; AEnemyItemSiteDirector spawns it on a field map.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldMissionEnemyArmy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Army")
	EWorldFieldDivisions DivisionType = EWorldFieldDivisions::None;

	// One entry per vehicle; duplicates are several vehicles of that type.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Army")
	TArray<ETankSubtype> Vehicles;

	// One entry per squad; duplicates are several squads of that type.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Army")
	TArray<ESquadSubtype> Squads;

	// Supply convoys have no roster, only a number of trucks.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Army", meta = (ClampMin = "0"))
	int32 ConvoyTruckCount = 0;

	int32 GetUnitCount() const { return Vehicles.Num() + Squads.Num() + FMath::Max(0, ConvoyTruckCount); }
};
