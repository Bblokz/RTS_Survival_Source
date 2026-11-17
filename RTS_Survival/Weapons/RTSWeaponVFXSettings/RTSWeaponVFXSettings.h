// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/MuzzleVfx/SmallArmsMuzzleClass.h"
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

	/**
	 * The one Niagara system that draws the muzzle flash and powder smoke of every infantry small arm.
	 * Emitters are named Flash_<Layer> / Smoke_<Layer>; built by the console command RTS.MuzzleVfx.PatchNiagaraSystem.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Small Arms Muzzle VFX")
	TSoftObjectPtr<UNiagaraSystem> SmallArmsMuzzleNiagaraSystem;

	/**
	 * The one Niagara system that draws every aircraft strafe impact (dirt spurts, debris, dust, sparks on units).
	 * Emitters are named Ground_<Layer> / Unit_<Layer>; built by the console command RTS.StrafeImpactVfx.BuildNiagaraSystem.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Aircraft Strafe VFX")
	TSoftObjectPtr<UNiagaraSystem> AircraftStrafeImpactNiagaraSystem;

	// Strafe impacts above this amount in one frame are not drawn.
	UPROPERTY(EditAnywhere, Config, Category="Aircraft Strafe VFX", meta=(ClampMin="1", UIMin="1"))
	int32 MaxAircraftStrafeImpactsPerFrame = 512;

	// Off: infantry weapons keep the launch effect configured on each weapon.
	UPROPERTY(EditAnywhere, Config, Category="Small Arms Muzzle VFX")
	bool bUseBatchedSmallArmsMuzzleVfx = true;

	// Shots above this amount in one frame are not drawn; keeps a huge fight from flooding the system.
	UPROPERTY(EditAnywhere, Config, Category="Small Arms Muzzle VFX", meta=(ClampMin="1", UIMin="1"))
	int32 MaxSmallArmsMuzzleShotsPerFrame = 128;

	/**
	 * Only weapons whose own launch effect is one of these are drawn by the batcher (and only when their weapon
	 * name has a muzzle class); a weapon with any other launch effect keeps it.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Small Arms Muzzle VFX")
	TArray<TSoftObjectPtr<UNiagaraSystem>> SmallArmsLaunchEffectsToBatch;

	// How the flash and smoke of each class of small arms look; classes come from the weapon name.
	UPROPERTY(EditAnywhere, Config, Category="Small Arms Muzzle VFX")
	TMap<ESmallArmsMuzzleClass, FSmallArmsMuzzleProfile> SmallArmsMuzzleProfiles;

	/** @return The profile of a muzzle class; the built-in default when the map has no entry for it. */
	FSmallArmsMuzzleProfile GetSmallArmsMuzzleProfile(const ESmallArmsMuzzleClass MuzzleClass) const;

	/** @return Whether a weapon with this launch effect may be drawn by the small arms muzzle batcher. */
	bool GetIsSmallArmsLaunchEffectToBatch(const UNiagaraSystem* LaunchEffect) const;

	/** Convenience accessor. */
	static const URTSWeaponVFXSettings* Get();

	/**
	 * @brief Resolve a color for a given shell type.
	 * @return Configured color or Transparent if not configured.
	 */
	UFUNCTION(BlueprintCallable, Category="Shell VFX")
	FLinearColor ResolveColorForShell(const EWeaponShellType ShellType) const;
};
