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

	// Add-on armour panels (spaced armour, skirts, fences) as installed by UAddOnArmorPanelsComponent; zero when the
	// unit has none. Lowest and highest add-on mm over the plates the panels cover (equal for most vehicles).
	UPROPERTY()
	float MinAddOnArmorMm = 0.f;

	UPROPERTY()
	float MaxAddOnArmorMm = 0.f;

	// How many panels the hull carries, from its panel layout; 0 when unknown.
	UPROPERTY()
	int32 AddOnArmorPanelCount = 0;

	// Which hull faces the panels cover (EArmorPlateDamageType of the covered plates).
	UPROPERTY()
	bool bAddOnArmorCoversFront = false;

	UPROPERTY()
	bool bAddOnArmorCoversSides = false;

	UPROPERTY()
	bool bAddOnArmorCoversRear = false;

	bool GetHasAddOnArmor() const { return MaxAddOnArmorMm > 0.f; }
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

	/**
	 * @brief The weapons of a live unit as the variant screen lists them (identical weapons merged), for screens that
	 * show a unit already in the world, such as the vehicle info takeover.
	 * @param Unit A tank or building expansion whose BeginPlay weapon setup has run.
	 * @return Empty for other actors or a unit without weapons.
	 */
	static TArray<FUnitVariantWeaponStats> ReadWeaponStats(const AActor& Unit);

	/**
	 * @brief Merges weapons the caller collected itself (identical weapons counted), for units the probe does not know,
	 * such as the soldiers of a squad in the squad info takeover.
	 * @param WeaponStates Weapons whose BeginPlay setup has run; null entries are skipped.
	 * @return One entry per distinct weapon.
	 */
	static TArray<FUnitVariantWeaponStats> ReadWeaponStatsFromStates(const TArray<const UWeaponState*>& WeaponStates);

	/**
	 * @brief A hidden, frozen probe for screens that time their own reads (the tech wheel reads a vehicle, applies a
	 * technology and reads it again). The caller waits for the BeginPlay setup and destroys the probe.
	 * @param World Game world to spawn in.
	 * @param UnitClass Loaded unit class.
	 * @return Null when the spawn failed.
	 */
	static AActor* SpawnHiddenProbe(UWorld& World, UClass* UnitClass) { return SpawnProbe(World, UnitClass); }

	/** @return What ProbeClasses would store for this probe, read now. */
	static FUnitVariantProbeResult ReadHiddenProbe(const AActor& Probe) { return ReadProbe(Probe); }

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
	/** Fills the add-on armour fields of Result when the probe's panel zones installed add-on armour. */
	static void ReadAddOnArmor(const AActor& Probe, FUnitVariantProbeResult& Result);
};
