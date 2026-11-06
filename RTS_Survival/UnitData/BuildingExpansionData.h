#pragma once

#include "CoreMinimal.h"

#include "UnitCost.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"

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
        TArray<FUnitAbilityEntry> Abilities;

	UPROPERTY(BlueprintReadOnly)
	 FResistanceAndDamageReductionData ResistancesAndDamageMlt;

	// Storage the expansion adds to its owning building's resource drop off while it is built (removed again when it
	// packs up or dies). Empty for expansions that add no storage.
	UPROPERTY(BlueprintReadOnly)
	TMap<ERTSResourceType, int32> ExtraResourceStorage;
};
