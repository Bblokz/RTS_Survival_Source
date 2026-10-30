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

	// True for the faction HQ: always in slot 0, cannot be removed and costs no supply.
	UPROPERTY(BlueprintReadOnly, Category = "Army")
	bool bIsHQ = false;
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
 * @brief Army selection rules; lives on the survival settings asset.
 */
USTRUCT(BlueprintType)
struct FSurvivalArmySettings
{
	GENERATED_BODY()

	// Total slots including the locked HQ slot; the selection UI wraps them into as many rows as needed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army", meta = (ClampMin = "2", ClampMax = "256"))
	int32 SlotCount = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army", meta = (ClampMin = "0"))
	int32 SupplyBudget = 100;

	// The HQ every army starts with; factions without an entry use Nomadic_GerHq.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army")
	TMap<ERTSFaction, FTrainingOption> HQPerFaction;

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
