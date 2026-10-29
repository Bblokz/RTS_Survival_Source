// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "UnitVariantProbes.generated.h"

class AActor;
class UWeaponState;

/** One weapon type of a probed variant, as the weapon bar would show it; identical weapons are merged. */
USTRUCT()
struct FUnitVariantWeaponStats
{
	GENERATED_BODY()

	// EWeaponName as its enum string: the WeaponUI_Data row name and the key two variants are compared by.
	UPROPERTY()
	FName WeaponRowName;

	UPROPERTY()
	FText DisplayName;

	// How many of this weapon the unit carries (twin guns, coaxial pairs).
	UPROPERTY()
	int32 Count = 0;

	UPROPERTY()
	float CalibreMm = 0.f;

	UPROPERTY()
	float Damage = 0.f;

	UPROPERTY()
	float ArmorPen = 0.f;

	UPROPERTY()
	float ReloadSeconds = 0.f;

	UPROPERTY()
	float RangeMeters = 0.f;

	UPROPERTY()
	int32 MagCapacity = 0;
};

/** What a probe read off a spawned variant once its BeginPlay (and the delays in it) had run. */
USTRUCT()
struct FUnitVariantProbeResult
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FUnitVariantWeaponStats> Weapons;

	UPROPERTY()
	float MaxHealth = 0.f;

	// FTankVariantMobilityMultipliers.MaxSpeedMultiplier; 1 for units that are not tanks.
	UPROPERTY()
	float MaxSpeedMultiplier = 1.f;
};

/**
 * @brief Weapons and health are set up by Blueprint graphs at BeginPlay, so the unit variant screen learns them by
 * spawning each variant once, hidden and frozen far below the map, reading it after its delayed setup and
 * destroying it. Results are kept per class for the rest of the screen's life. Owned by UW_UnitVariantSelectionMenu.
 */
UCLASS()
class RTS_SURVIVAL_API UUnitVariantProbes : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @brief Spawns a probe of every class not probed yet; calls OnProbesRead once they are read (at once when all
	 * classes are known). A new call cancels probes that are still settling.
	 * @param World Game world to spawn in.
	 * @param UnitClasses Loaded unit classes.
	 * @param OnProbesRead Called on the game thread when every class has a result.
	 */
	void ProbeClasses(UWorld& World, const TArray<UClass*>& UnitClasses, TFunction<void()> OnProbesRead);

	/** @return Null when the class was not probed (yet). */
	const FUnitVariantProbeResult* FindResult(const UClass* UnitClass) const;

	/** Destroys probes that are still settling, without reading them; the menu calls it when it closes. */
	void CancelProbes();

private:
	UPROPERTY()
	TMap<TObjectPtr<UClass>, FUnitVariantProbeResult> M_Results;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_LiveProbes;

	TWeakObjectPtr<UWorld> M_World;
	FTimerHandle M_SettleTimer;
	TFunction<void()> M_OnProbesRead;

	static AActor* SpawnProbe(UWorld& World, UClass* UnitClass);
	void ReadAndDestroyProbes();
	static FUnitVariantProbeResult ReadProbe(const AActor& Probe);
	static TArray<UWeaponState*> GetProbeWeapons(const AActor& Probe);
	static void AddWeapon(TArray<FUnitVariantWeaponStats>& Weapons, const UWeaponState& WeaponState);
};
