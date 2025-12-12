// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponShellType/WeaponShellType.h"
#include "RTSWeaponVFXSettings.generated.h"

/**
 * @brief Central place to map shell types to designer-tunable VFX colors.
 * Fill this in Project Settings → Game → RTS Weapon VFX.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Weapon VFX"))
class RTS_SURVIVAL_API URTSWeaponVFXSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Per-shell VFX color for UI/FX usage. */
	UPROPERTY(EditAnywhere, Config, Category="Shell VFX")
	TMap<EWeaponShellType, FLinearColor> ShellTypeColors;

	UPROPERTY(EditAnywhere, Config, Category="Shell VFX")
	TMap<EWeaponShellType, FLinearColor> ShellTypeGroundSmokeColors;
	/** Convenience accessor. */
	static const URTSWeaponVFXSettings* Get();

	/**
	 * @brief Resolve a color for a given shell type.
	 * @return Configured color or Transparent if not configured.
	 */
	UFUNCTION(BlueprintCallable, Category="Shell VFX")
	FLinearColor ResolveColorForShell(const EWeaponShellType ShellType) const;
};
