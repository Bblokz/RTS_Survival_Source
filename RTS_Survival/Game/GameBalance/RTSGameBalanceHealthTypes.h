#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EGameBalanceHealthTypes : uint8
{
	HT_None UMETA(DisplayName = "None"),
	HT_LightTank UMETA(DisplayName = "Light Tank"),
	HT_LightMediumTank UMETA(DisplayName = "Light Medium Tank"),
	HT_MediumTank UMETA(DisplayName = "Medium Tank"),
	HT_T2HeavyTank UMETA(DisplayName = "T2 Heavy Tank"),
	HT_T3MediumTank UMETA(DisplayName = "T3 Medium Tank"),
	HT_T3HeavyTank UMETA(DisplayName = "T3 Heavy Tank"),
	HT_LightTankShot UMETA(DisplayName = "Light Tank Shot"),
	HT_MediumTankShot UMETA(DisplayName = "Medium Tank Shot"),
	HT_HeavyTankShot UMETA(DisplayName = "Heavy Tank Shot"),

	HT_BasicInfantry UMETA(DisplayName = "Basic Infantry"),
	HT_ArmoredInfantry UMETA(DisplayName = "Armored Infantry"),
	HT_T2Infantry UMETA(DisplayName = "T2 Infantry"),
	HT_EliteInfantry UMETA(DisplayName = "Elite Infantry"),
};