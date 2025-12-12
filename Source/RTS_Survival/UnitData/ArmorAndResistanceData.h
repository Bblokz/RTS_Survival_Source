#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/Resistances/Resistances.h"
#include "RTS_Survival/RTSComponents/DamageReduction/DamageReduction.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarIcons/HealthBarIcons.h"

#include "ArmorAndResistanceData.generated.h"


USTRUCT(BlueprintType, Blueprintable)
struct FFireResistanceData
{
	GENERATED_BODY()

	// The value at which the fire damage maxes out and starts damaging the HP of the unit.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float MaxFireThreshold = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float FireRecoveryPerSecond = 0.0f;
	
};

UENUM(BlueprintType)
enum class EResistancePresetType : uint8
{
	None,
	BasicInfantryArmor,
	HeavyInfantryArmor,
	CommandoInfantryArmor,
	HazmatInfantryArmor,
	ArmoredCar,
	LightArmor,
	MediumArmor,
	HeavyArmor,
	BasicBuildingArmor,
	ReinforcedBuildingArmor,
};

// Defines the values for resistance to damage types other than kinetic.
// To get the resistance data of a unit call FUnitResistanceDataHelpers::GetUnitResistanceData
USTRUCT(BlueprintType, Blueprintable)
struct FResistanceAndDamageReductionData
{
	GENERATED_BODY()

	// Has a multiplier per amor plate type.
	UPROPERTY(BlueprintReadWrite)
	FLaserRadiationDamageMlt LaserAndRadiationDamageMlt;

	UPROPERTY(BlueprintReadWrite)
	FFireResistanceData FireResistanceData;

	// Contains a multiplier for all damage types as well as a subtraction value.
	UPROPERTY(BlueprintReadWrite)
	FDamageReductionSettings AllDamageReductionSettings;

	UPROPERTY(BlueprintReadWrite)
	ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::None;
	
};
