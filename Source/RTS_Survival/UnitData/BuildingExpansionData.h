#pragma once

#include "CoreMinimal.h"

#include "UnitCost.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"

#include "BuildingExpansionData.generated.h"


USTRUCT(Blueprintable)
struct FBxpData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float ConstructionTime = 5.f;

	UPROPERTY(BlueprintReadOnly)
	float Health = 100.f;
	
	UPROPERTY(BlueprintReadOnly)
	float VisionRadius = 0.f;

	UPROPERTY(BlueprintReadOnly)
	FUnitCost Cost;

	UPROPERTY(BlueprintReadOnly)
	int32 EnergySupply = 0;

	// Readwrite for array references in blueprint.
	UPROPERTY(BlueprintReadWrite)
	TArray<EAbilityID> Abilities;

	UPROPERTY(BlueprintReadOnly)
	 FResistanceAndDamageReductionData ResistancesAndDamageMlt;
};
