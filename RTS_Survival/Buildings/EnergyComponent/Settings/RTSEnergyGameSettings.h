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

	UMaterialInterface* GetLowEnergyMaterial() const;
	TSubclassOf<ULowEnergyBehaviour> GetLowEnergyBehaviourClass() const;

private:
	UPROPERTY(Transient)
	mutable TObjectPtr<UMaterialInterface> M_CachedLowEnergyMaterial = nullptr;
};
