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

	/** M_UI_IndicatorLampHalo: the light an idle lamp throws on the HUD around it, pulsing with the lamp. Empty: none. */
	UPROPERTY(Config, EditAnywhere, Category="Idle Unit Buttons")
	TSoftObjectPtr<UMaterialInterface> IdleLampHaloMaterial;

	/** Edge of the idle lamp's halo over the edge of the lamp. */
	UPROPERTY(Config, EditAnywhere, Category="Idle Unit Buttons", meta=(ClampMin="1.0"))
	float IdleLampHaloScale = 3.5f;

	/** Seconds a lamp takes to light up or go dark, like a warming filament. */
	UPROPERTY(Config, EditAnywhere, Category="Lamps", meta=(ClampMin="0.0"))
	float FadeSeconds = 0.1f;

	/**
	 * M_UI_SignalLamp instance for the page lamps of every info takeover: the lamp and a halo that lights the panel
	 * around it. Empty falls back to the takeover style's flat PageLampLit / PageLampUnlit brushes.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Info Takeover Page Lamps")
	TSoftObjectPtr<UMaterialInterface> PageLampMaterial;

	/** Edge of the halo over the edge of the lamp; the material fades the halo out before the halo's edge. */
	UPROPERTY(Config, EditAnywhere, Category="Info Takeover Page Lamps", meta=(ClampMin="1.0"))
	float PageLampHaloScale = 3.f;

	/** Seconds a page lamp takes to light up or go dark when the page changes. */
	UPROPERTY(Config, EditAnywhere, Category="Info Takeover Page Lamps", meta=(ClampMin="0.0"))
	float PageLampFadeSeconds = 0.15f;
};
