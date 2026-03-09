#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTSGameSettings.generated.h"


UENUM(BlueprintType)
enum class ERTSGameSetting : uint8
{
	None UMETA(DisplayName = "None"),
	UseDeselectedDecals UMETA(Displayname = "Use Deselected Decals"),

	StartFuel UMETA(DisplayName = "Start Fuel"),
	StartAmmo UMETA(DisplayName = "Start Ammo"),

	StartWeaponBlueprints UMETA(DisplayName = "Start Weapon Blueprints"),
	StartVehicleBlueprints UMETA(DisplayName = "Start Vehicle Blueprints"),
	StartEnergyBlueprints UMETA(DisplayName = "Start Energy Blueprints"),
	StartConstructionBlueprints UMETA(DisplayName = "Start Construction Blueprints"),

	StartBlueprintsStorage UMETA(DisplayName = "Start Blueprints Storage")
};

USTRUCT()
struct FRTSGameSettings
{
	GENERATED_BODY()

	FRTSGameSettings()
		: bUseDeselectedDecals(false),
		  StartFuel(FMath::Clamp(100, 0, 300)),
		  StartAmmo(FMath::Clamp(200, 0, 500)),
		  StartWeaponBlueprints(FMath::Clamp(0, 0, 100)),
		  StartVehicleBlueprints(FMath::Clamp(0, 0, 100)),
		  StartEnergyBlueprints(FMath::Clamp(0, 0, 100)),
		  StartConstructionBlueprints(FMath::Clamp(0, 0, 100)),
		  StartBlueprintsStorage(FMath::Clamp(INT32_MAX, 0, INT32_MAX))
	{
	}

	bool bUseDeselectedDecals;
	int32 StartFuel;
	int32 StartAmmo;

	int32 StartWeaponBlueprints;
	int32 StartVehicleBlueprints;
	int32 StartEnergyBlueprints;
	int32 StartConstructionBlueprints;

	// Same for all blueprints
	int32 StartBlueprintsStorage;
};
