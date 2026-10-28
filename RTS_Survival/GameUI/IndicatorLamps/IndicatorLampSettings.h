// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "IndicatorLampSettings.generated.h"

class UMaterialInterface;

/**
 * @brief Project settings for the analog indicator lamps on the HUD (M_UI_IndicatorLamp instances, see
 * Source/Docs/UI/AnalogGauges.md): one on each idle unit button. No HUD Blueprint needs its own setup.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Indicator Lamps"))
class RTS_SURVIVAL_API UIndicatorLampSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UIndicatorLampSettings();

	/** Blinks on the idle harvester button while a harvester is idle. */
	UPROPERTY(Config, EditAnywhere, Category="Idle Unit Buttons")
	TSoftObjectPtr<UMaterialInterface> IdleHarvesterLamp;

	/** Blinks on the idle nomadic truck button while a truck is idle. */
	UPROPERTY(Config, EditAnywhere, Category="Idle Unit Buttons")
	TSoftObjectPtr<UMaterialInterface> IdleNomadTruckLamp;

	/** Edge length of the idle lamp in UI units; it sits in the button's free top-right corner. */
	UPROPERTY(Config, EditAnywhere, Category="Idle Unit Buttons", meta=(ClampMin="4.0"))
	float IdleLampSize = 14.f;

	/** How far the idle lamp reaches past the button's top and right edge, in UI units. */
	UPROPERTY(Config, EditAnywhere, Category="Idle Unit Buttons", meta=(ClampMin="0.0"))
	float IdleLampOverhang = 4.f;

	/** Seconds a lamp takes to light up or go dark, like a warming filament. */
	UPROPERTY(Config, EditAnywhere, Category="Lamps", meta=(ClampMin="0.0"))
	float FadeSeconds = 0.1f;
};
