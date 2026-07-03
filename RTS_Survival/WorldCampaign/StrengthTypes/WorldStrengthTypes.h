#pragma once

#include "CoreMinimal.h"

#include "WorldStrengthTypes.generated.h"

UENUM(BlueprintType)
enum class EWorldStrengthTypes : uint8
{
	None,
	FortificationStrength,
	FieldDivisions,
	StrategicSupport
};

UENUM(BlueprintType)
enum class EWorldFortificationStrength : uint8
{
	None,
	Bunkers,
	Minefields,
	AABattery
};

UENUM(BlueprintType)
enum class EWorldFieldDivisions : uint8
{
	None,
	SovietLightArmorDivision,
	SovietMediumArmorDivision,
	SovietHeavyArmorDivision,
	SovietInfantryDivision,
};

UENUM(BlueprintType)
enum class EWorldStrategicSupport : uint8
{
	None,
	AirSupport
};
