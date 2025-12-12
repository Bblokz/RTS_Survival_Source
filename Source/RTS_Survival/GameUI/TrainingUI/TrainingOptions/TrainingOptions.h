// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "TrainingOptions.generated.h"

class URTSRequirement;
class UTechRequirement;

/**
* @note For accessing the training data in the DT_Training datatable we convert the training option into a name to access the row.
* @note For this we use FString UTrainingOptionLibrary::GetTrainingOptionName(EAllUnitType UnitType, uint8 SubtypeValue)
* @note 
*/
USTRUCT(BlueprintType)
struct FTrainingOption
{
	GENERATED_BODY()

public:
	FString GetTrainingName() const;
	FString GetDisplayName() const;

	/** @return Whether this training option is not initialized */
	bool IsNone() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EAllUnitType UnitType;

	// Use a uint8 to hold the subtype value
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 SubtypeValue;


	FTrainingOption()
		: UnitType(EAllUnitType::UNType_None)
		  , SubtypeValue(0)
	{
	}

	FTrainingOption(EAllUnitType InUnitType, uint8 InSubtypeValue)
		: UnitType(InUnitType)
		  , SubtypeValue(InSubtypeValue)
	{
	}

	// Equality operator for TMap key usage
	bool operator==(const FTrainingOption& Other) const
	{
		return UnitType == Other.UnitType && SubtypeValue == Other.SubtypeValue;
	}

	void Reset()
	{
		UnitType = EAllUnitType::UNType_None;
		SubtypeValue = 0;
	}
};

// Implement GetTypeHash for FTrainingOption
FORCEINLINE uint32 GetTypeHash(const FTrainingOption& Option)
{
	return HashCombine(GetTypeHash(static_cast<uint8>(Option.UnitType)), GetTypeHash(Option.SubtypeValue));
}

// Defines what the training option is, whether it is unlocked or not.
USTRUCT(BlueprintType)
struct FTrainingOptionState
{
	GENERATED_BODY()

	// The training option that identifies this item.
	UPROPERTY(BlueprintReadWrite)
	FTrainingOption ItemID;

	UPROPERTY(BlueprintReadWrite)
	int32 TrainingTime;

	// Indicates the type of unit.
	UPROPERTY(BlueprintReadWrite)
	EAllUnitType UnitType;

	// Holds the subtype based on UnitType.
	UPROPERTY(BlueprintReadWrite)
	ETankSubtype TankSubtype;

	UPROPERTY(BlueprintReadWrite)
	ENomadicSubtype NomadicSubtype;

	UPROPERTY(BlueprintReadWrite)
	ESquadSubtype SquadSubtype;

	UPROPERTY(BlueprintReadWrite)
	EAircraftSubtype AircraftSubType;

	// Whether this item is unlocked/locked. If not set, we assume the item is unlocked.
	UPROPERTY()
	TObjectPtr<URTSRequirement> RTSRequirement;

	// Constructor to initialize members to default values.
	FTrainingOptionState()
		: ItemID(),
		  TrainingTime(0),
		  UnitType(EAllUnitType::UNType_None),
		  TankSubtype(ETankSubtype::Tank_None),
		  NomadicSubtype(ENomadicSubtype::Nomadic_None),
		  SquadSubtype(ESquadSubtype::Squad_None),
		  AircraftSubType(EAircraftSubtype::Aircarft_None),
		  RTSRequirement(nullptr)
	{
	}
};
