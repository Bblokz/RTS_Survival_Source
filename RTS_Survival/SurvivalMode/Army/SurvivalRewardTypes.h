// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurvivalArmyTypes.h"

#include "SurvivalRewardTypes.generated.h"

/**
 * @brief What kind of reinforcement set an option is; the three options of a reward always differ.
 */
UENUM(BlueprintType)
enum class ESurvivalRewardTheme : uint8
{
	// Rifle, assault and elite squads.
	Infantry,
	// Tanks and armoured cars.
	Armour,
	// Team weapons, harvesters and trucks.
	Support,
	// Anything from the roster.
	Combined,
	// A designer-authored set from FSurvivalRewardSettings::AuthoredSets.
	Authored
};

/**
 * @brief A fixed reinforcement set a designer can add to the reward pool.
 */
USTRUCT(BlueprintType)
struct FSurvivalAuthoredRewardSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	FText Title;

	// Units of the set; each is flown in as the variant the player selected.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TArray<FTrainingOption> Units;

	// The set can only be offered from this round on.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "1"))
	int32 MinRound = 1;

	// Chance that one of the three options is replaced by an authored set (summed over eligible sets, max 1).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OfferChance = 0.15f;
};

/**
 * @brief The intermission between waves and the roguelike reinforcement rewards offered in it.
 */
USTRUCT(BlueprintType)
struct FSurvivalRewardSettings
{
	GENERATED_BODY()

	// Short grace between two waves; the player picks a reward in it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intermission", meta = (ClampMin = "5.0"))
	float IntermissionSeconds = 40.f;

	// Later intermissions can be a little longer (more units to reorganise); 0 keeps them equal.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intermission", meta = (ClampMin = "0.0"))
	float IntermissionSecondsPerRound = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intermission", meta = (ClampMin = "5.0"))
	float MaxIntermissionSeconds = 60.f;

	// Number of reinforcement sets offered after each survived wave.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "1", ClampMax = "6"))
	int32 OptionsPerReward = 3;

	// Supply worth of a set after the first wave; grows per round so later rewards matter against bigger waves.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "1"))
	int32 FirstRewardSupply = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "0"))
	int32 RewardSupplyPerRound = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "1"))
	int32 MaxRewardSupply = 70;

	// At most this many units in one set (a set of 12 rifle squads is not fun to land).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "1"))
	int32 MaxUnitsPerSet = 6;

	// When the intermission ends without a pick the first option is flown in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	bool bAutoPickWhenTimeRunsOut = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (TitleProperty = "Title"))
	TArray<FSurvivalAuthoredRewardSet> AuthoredSets;
};

/**
 * @brief One of the reinforcement sets the player can pick between waves.
 */
USTRUCT(BlueprintType)
struct FSurvivalRewardOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	ESurvivalRewardTheme Theme = ESurvivalRewardTheme::Combined;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TArray<FSurvivalArmyEntry> Units;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int32 TotalSupply = 0;
};
