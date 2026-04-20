// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VeterancyFXSettings.generated.h"

class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * @brief Defines the shared veterancy FX assets used by the game state cache.
 *
 * Configure these settings once in project settings so level-up FX can be reused
 * without spawning new components at runtime.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Veterancy FX"))
class RTS_SURVIVAL_API UVeterancyFXSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVeterancyFXSettings();

	UPROPERTY(EditAnywhere, Config, Category="Veterancy FX")
	TSoftObjectPtr<UNiagaraSystem> M_VeterancyNiagaraSystem;

	UPROPERTY(EditAnywhere, Config, Category="Veterancy FX")
	TSoftObjectPtr<USoundBase> M_VeterancySound;

	UPROPERTY(EditAnywhere, Config, Category="Veterancy FX")
	TSoftObjectPtr<USoundAttenuation> M_VeterancySoundAttenuation;

	UPROPERTY(EditAnywhere, Config, Category="Veterancy FX")
	TSoftObjectPtr<USoundConcurrency> M_VeterancySoundConcurrency;

	UPROPERTY(EditAnywhere, Config, Category="Veterancy FX", meta=(ClampMin="0.01"))
	float M_VeterancyFXActiveDuration = 2.0f;

	static const UVeterancyFXSettings* Get();
};
