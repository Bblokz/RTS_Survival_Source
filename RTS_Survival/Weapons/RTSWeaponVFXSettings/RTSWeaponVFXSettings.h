// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponShellType/WeaponShellType.h"
#include "RTSWeaponVFXSettings.generated.h"

class UNiagaraSystem;

/**
 * @brief Central place to map shell types to designer-tunable VFX colors.
 * Fill this in Project Settings → Game → RTS Weapon VFX.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Weapon VFX"))
class RTS_SURVIVAL_API URTSWeaponVFXSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSWeaponVFXSettings();

	/** Per-shell VFX color for UI/FX usage. */
	UPROPERTY(EditAnywhere, Config, Category="Shell VFX")
	TMap<EWeaponShellType, FLinearColor> ShellTypeColors;

	UPROPERTY(EditAnywhere, Config, Category="Shell VFX")
	TMap<EWeaponShellType, FLinearColor> ShellTypeGroundSmokeColors;

	/**
	 * The one Niagara system that draws the impacts of every trace weapon (infantry weapons, hull MGs).
	 * Has one group of emitters per impact type and reads the impacts of a frame from user arrays.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Trace Impact VFX")
	TSoftObjectPtr<UNiagaraSystem> TraceImpactNiagaraSystem;

	// Off: trace weapons fall back to the impact systems configured on each weapon.
	UPROPERTY(EditAnywhere, Config, Category="Trace Impact VFX")
	bool bUseCentralisedTraceImpactVfx = true;

	// Impacts above this amount in one frame are not drawn; keeps a huge fight from flooding the system.
	UPROPERTY(EditAnywhere, Config, Category="Trace Impact VFX", meta=(ClampMin="1", UIMin="1"))
	int32 MaxTraceImpactsPerFrame = 256;

	/** Convenience accessor. */
	static const URTSWeaponVFXSettings* Get();

	/**
	 * @brief Resolve a color for a given shell type.
	 * @return Configured color or Transparent if not configured.
	 */
	UFUNCTION(BlueprintCallable, Category="Shell VFX")
	FLinearColor ResolveColorForShell(const EWeaponShellType ShellType) const;
};
