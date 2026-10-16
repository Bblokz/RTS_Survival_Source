// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "RTSEnergyGameSettings.generated.h"

class ULowEnergyBehaviour;
class UMaterialInterface;

/**
 * @brief Project settings that define low energy visuals and behaviour used by energy components.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Energy Game Settings"))
class RTS_SURVIVAL_API URTSEnergyGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSEnergyGameSettings();

	UPROPERTY(EditAnywhere, Config, Category = "Energy")
	TSoftObjectPtr<UMaterialInterface> LowEnergyMaterial;

	UPROPERTY(EditAnywhere, Config, Category = "Energy")
	TSubclassOf<ULowEnergyBehaviour> LowEnergyBehaviourClass;

	/**
	 * @brief Loads the material on first use. Not cached here: the settings default object is never traced by the
	 * garbage collector, so a cached pointer could outlive the material; callers keep it in their own UPROPERTY.
	 * @return The low energy material, or null when none is set.
	 */
	UMaterialInterface* GetLowEnergyMaterial() const;
	TSubclassOf<ULowEnergyBehaviour> GetLowEnergyBehaviourClass() const;
};
