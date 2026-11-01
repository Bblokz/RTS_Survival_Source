// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Variants/UnitVariantTypes.h"

#include "SurvivalArmyTypes.generated.h"

/**
 * @brief Designer supply cost for one unit; variants can cost more or less than the base unit.
 */
USTRUCT(BlueprintType)
struct FSurvivalUnitSupplyCost
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply")
	FTrainingOption Unit;

	// Name shown in the army and reward screens; empty uses the training option name.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply", meta = (ClampMin = "0"))
	int32 BaseSupply = 10;

	// Absolute supply cost for a variant; a variant without an entry costs BaseSupply.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply")
	TMap<EUnitVariant, int32> VariantSupply;

	// 0 means the player can put as many copies in the army as the slots and supply allow.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply", meta = (ClampMin = "0"))
	int32 MaxCopies = 0;
};

/**
 * @brief One unit the player can drag into the army: the faction unit as the variant the player selected.
 */
USTRUCT(BlueprintType)
struct FSurvivalArmyEntry
{
	GENERATED_BODY()

	// The unit as the training layout and the async spawner key it.
	UPROPERTY(BlueprintReadOnly, Category = "Army")
	FTrainingOption Unit;

	// The variant the player picked in the variant menu; Base when none.
	UPROPERTY(BlueprintReadOnly, Category = "Army")
	EUnitVariant Variant = EUnitVariant::Base;

	UPROPERTY(BlueprintReadOnly, Category = "Army")
	int32 SupplyCost = 0;

	// 0 = unlimited.
	UPROPERTY(BlueprintReadOnly, Category = "Army")
	int32 MaxCopies = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Army")
	FText DisplayName;

	// True for the faction HQ: always in slot 0, cannot be removed and costs no supply. Also bIsLocked.
	UPROPERTY(BlueprintReadOnly, Category = "Army")
	bool bIsHQ = false;

	// True for the HQ and the fixed start units after it (harvesters, scavengers): they fill the first slots,
	// cannot be removed or moved and cost no supply.
	UPROPERTY(BlueprintReadOnly, Category = "Army")
	bool bIsLocked = false;
};

/**
 * @brief What the player put in one army slot.
 */
USTRUCT(BlueprintType)
struct FSurvivalArmyPick
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Army")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Army")
	FSurvivalArmyEntry Entry;
};

/**
 * @brief A unit every army starts with in locked slots right after the HQ, Count times.
 */
USTRUCT(BlueprintType)
struct FSurvivalLockedStartUnit
{
	GENERATED_BODY()

	FSurvivalLockedStartUnit() = default;

	FSurvivalLockedStartUnit(const FTrainingOption& InUnit, const int32 InCount)
		: Unit(InUnit)
		, Count(InCount)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army")
	FTrainingOption Unit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army", meta = (ClampMin = "0", ClampMax = "64"))
	int32 Count = 1;
};

/**
 * @brief The locked start units of one faction (map value wrapper for FSurvivalArmySettings).
 */
USTRUCT(BlueprintType)
struct FSurvivalLockedStartUnits
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army", meta = (TitleProperty = "Unit"))
	TArray<FSurvivalLockedStartUnit> Units;
};

/**
 * @brief Army selection rules; lives on the survival settings asset.
 */
USTRUCT(BlueprintType)
struct FSurvivalArmySettings
{
	GENERATED_BODY()

	// Slots including the locked HQ slot; the locked start units get extra slots on top, so the player always has
	// SlotCount - 1 free slots. The selection UI wraps them into as many rows as needed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army", meta = (ClampMin = "2", ClampMax = "256"))
	int32 SlotCount = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army", meta = (ClampMin = "0"))
	int32 SupplyBudget = 100;

	// The HQ every army starts with; factions without an entry use Nomadic_GerHq.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army")
	TMap<ERTSFaction, FTrainingOption> HQPerFaction;

	// Units every army starts with after the HQ, locked and free of supply, like the HQ.
	// Factions without an entry use DefaultLockedStartUnits.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army")
	TMap<ERTSFaction, FSurvivalLockedStartUnits> LockedStartUnitsPerFaction;

	// 6 Pz I harvesters and 2 scavenger squads (the German economy units); harvesters keyed as the training layout
	// and the spawner key them (UNType_Tank).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army", meta = (TitleProperty = "Unit"))
	TArray<FSurvivalLockedStartUnit> DefaultLockedStartUnits = {
		FSurvivalLockedStartUnit(
			FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzI_Harvester)), 6),
		FSurvivalLockedStartUnit(
			FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_Scavengers)), 2)
	};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army", meta = (TitleProperty = "Unit"))
	TArray<FSurvivalUnitSupplyCost> UnitSupplyCosts;

	// Supply for faction units without an entry in UnitSupplyCosts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army")
	TMap<EAllUnitType, int32> FallbackSupplyPerUnitType;

	// Unit types the player can bring; building expansions and turrets cannot be flown in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army")
	TArray<EAllUnitType> AllowedUnitTypes = {
		EAllUnitType::UNType_Squad, EAllUnitType::UNType_Tank, EAllUnitType::UNType_Harvester,
		EAllUnitType::UNType_Nomadic, EAllUnitType::UNType_Aircraft
	};

	// Faction units that are never offered (e.g. a second HQ).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army")
	TArray<FTrainingOption> ExcludedUnits;
};
