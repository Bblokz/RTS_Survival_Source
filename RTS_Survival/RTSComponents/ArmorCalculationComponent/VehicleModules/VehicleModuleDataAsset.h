// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "VehicleModuleBalance.h"
#include "VehicleModuleBehaviour.h"
#include "RTS_Survival/Behaviours/Derived/Damage/TankEngineFire/TankEngineFireBehaviour.h"

#include "VehicleModuleDataAsset.generated.h"

class UTexture2D;

/** @brief Textures and optional behaviour classes of one module type; every field is independently optional. */
USTRUCT(BlueprintType)
struct FVehicleModuleStateAssets
{
	GENERATED_BODY()

	// Yellow icon shown on the tank healthbar.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vehicle Modules")
	TObjectPtr<UTexture2D> DamagedTexture = nullptr;

	// Red icon shown on the tank healthbar.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vehicle Modules")
	TObjectPtr<UTexture2D> DestroyedTexture = nullptr;

	// Null means the damaged state has no stat or capability effect.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vehicle Modules")
	TSubclassOf<UVehicleModuleBehaviour> DamagedBehaviourClass = nullptr;

	// Null means the destroyed state has no stat or capability effect.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vehicle Modules")
	TSubclassOf<UVehicleModuleBehaviour> DestroyedBehaviourClass = nullptr;
};

/**
 * @brief Shared module icons and optional state behaviours for every tank, selected in the Vehicle Modules
 * project settings; one entry per installable module type, including separate Tracks and Wheels entries.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UVehicleModuleDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vehicle Modules")
	TMap<EVehicleModuleTypes, FVehicleModuleStateAssets> ModulesByType;

	/** @brief Select Blueprint subclasses of TankEngineFireBehaviour for each vehicle profile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EngineFire")
	TSubclassOf<UTankEngineFireBehaviour> ArmoredCarEngineFire = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EngineFire")
	TSubclassOf<UTankEngineFireBehaviour> LightTankEngineFire = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EngineFire")
	TSubclassOf<UTankEngineFireBehaviour> MediumTankEngineFire = nullptr;

	// Super-heavy tanks share this fire behaviour.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EngineFire")
	TSubclassOf<UTankEngineFireBehaviour> HeavyTankEngineFire = nullptr;

	// Icon size in UMG units before the healthbar render scale.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vehicle Modules")
	FVector2D ImageSize = FVector2D(VehicleModuleBalance::DefaultModuleIconWidth,
	                                VehicleModuleBalance::DefaultModuleIconHeight);
};

/**
 * @brief Project settings reference to the shared module data asset, loaded once per game instance.
 * Appears under Project Settings as: Game ► Vehicle Modules.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Vehicle Modules"))
class RTS_SURVIVAL_API UVehicleModuleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Vehicle Modules")
	TSoftObjectPtr<UVehicleModuleDataAsset> ModuleDataAsset;

	static const UVehicleModuleSettings* Get()
	{
		return GetDefault<UVehicleModuleSettings>();
	}
};
