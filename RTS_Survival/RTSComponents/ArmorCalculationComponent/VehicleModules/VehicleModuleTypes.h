// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"

#include "VehicleModuleTypes.generated.h"

enum class EWeaponShellType : uint8;
class UMeshComponent;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EVehicleModuleTypes : uint8
{
	None,
	AddOnArmor,
	Tracks,
	Engine,
	Ammo,
	Turret,
	Weapon,
	Wheels
};

UENUM(BlueprintType)
enum class EVehicleModuleState : uint8
{
	Healthy,
	// Yellow.
	Damaged,
	// Red; recoverable while the tank survives.
	Destroyed
};

UENUM(BlueprintType)
enum class EVehicleModuleProfile : uint8
{
	ArmoredCar,
	LightTank,
	MediumTank,
	HeavyTank,
	SuperHeavyTank
};

UENUM(BlueprintType)
enum class EVehicleRunningGear : uint8
{
	Tracks,
	Wheels
};

// Selects which installed instance a plate candidate damages.
enum class EModuleTargetSelector : uint8
{
	None,
	// Engine and ammo: the only installed instance.
	Singleton,
	// Tracks or wheels on the struck side.
	RunningGearSide,
	// The turret whose registered mesh owns the struck plate.
	BoundTurret,
	// The sole weapon module, only on plates of its bound mesh.
	BoundWeapon,
	// The add-on armor zone configured to cover the struck plate.
	CoveringArmorZone
};

// Whether a non-penetrating hit may damage the candidate.
enum class EModuleNonPenPolicy : uint8
{
	Never,
	External,
	MantletOnly
};

enum class EModuleChangeCause : uint8
{
	Damage,
	// Ordinary healing restored red modules to yellow.
	Recovery,
	CrewRepair,
	// Ordinary healing completed the finishing work at full hull health.
	FullService,
	Load
};

// How an impact reached the vehicle; selects the source-specific candidate rules.
enum class EVehicleModuleDelivery : uint8
{
	Ballistic,
	Mine,
	Splash
};

// Keeps module splash opt-in so shared AOE callers (e.g. ICBMs) retain hull-only damage.
enum class EVehicleModuleSplashPolicy : uint8
{
	Ignore,
	DamageExternalModules
};

/** @brief One compile-time plate candidate; contains numeric data only. */
struct FPlateModuleDamage
{
	EVehicleModuleTypes TypeToDamage = EVehicleModuleTypes::None;
	float DamageMultiplier = 0.f;
	float DamageProbability = 0.f;
	EModuleTargetSelector TargetSelector = EModuleTargetSelector::None;
	EModuleNonPenPolicy NonPenPolicy = EModuleNonPenPolicy::Never;
};

/**
 * @brief Blueprint setup input for one module instance; module HP is derived from the tank's MaxHealth.
 * Turret and weapon modules bind the registered armor mesh that owns their plates.
 * Running gear binds a hull side instead of a mesh.
 */
USTRUCT(BlueprintType)
struct FVehicleModuleSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle Modules")
	EVehicleModuleTypes Type = EVehicleModuleTypes::None;

	// Registered armor mesh of the turret or gun; required for Turret and Weapon modules.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle Modules")
	TObjectPtr<UMeshComponent> BoundMesh = nullptr;

	// Hull side of a running-gear instance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle Modules")
	bool bRightSide = false;
};

/** @brief Transient setup input naming one registered mesh/plate pair covered by an add-on armor zone. */
USTRUCT(BlueprintType)
struct FAddOnArmorPlateBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle Modules")
	TObjectPtr<UMeshComponent> MeshWithArmor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle Modules")
	EArmorPlate PlateType = EArmorPlate::Plate_Front;
};

/** @brief Copied read-only view of one module instance. */
USTRUCT(BlueprintType)
struct FVehicleModuleSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Vehicle Modules")
	int32 ModuleId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Vehicle Modules")
	EVehicleModuleTypes Type = EVehicleModuleTypes::None;

	UPROPERTY(BlueprintReadOnly, Category="Vehicle Modules")
	EVehicleModuleState State = EVehicleModuleState::Healthy;

	UPROPERTY(BlueprintReadOnly, Category="Vehicle Modules")
	float CurrentHp = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Vehicle Modules")
	float MaxHp = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Vehicle Modules")
	bool bInstalled = false;
};

/** @brief Copied state-transition payload; safe to hold across callbacks because it owns no references. */
struct FModuleStateChange
{
	int32 ModuleId = INDEX_NONE;
	int32 SlotIndex = INDEX_NONE;
	EVehicleModuleTypes Type = EVehicleModuleTypes::None;
	EVehicleModuleState PreviousState = EVehicleModuleState::Healthy;
	EVehicleModuleState NewState = EVehicleModuleState::Healthy;
	float CurrentHp = 0.f;
	float MaxHp = 0.f;
	EModuleChangeCause Cause = EModuleChangeCause::Damage;
};

/**
 * @brief Explicit per-impact context; never cached on the component because impacts can re-enter.
 * Identity fields feed the deterministic candidate rolls.
 */
struct FVehicleModuleHitContext
{
	// Mesh slot * plates per mesh + plate index; INDEX_NONE when the hit plate is unknown.
	int32 StablePlateRegistrationId = INDEX_NONE;
	FVector HullLocalHitPosition = FVector::ZeroVector;
	bool bHasHullLocalHitPosition = false;
	EWeaponShellType ShellType = static_cast<EWeaponShellType>(0);
	ERTSDamageType DamageType = ERTSDamageType::Kinetic;
	EVehicleModuleDelivery DeliveryType = EVehicleModuleDelivery::Ballistic;
	bool bOverpenetrating = false;
	uint32 ShotActivationId = 0;
	uint32 ImpactOrdinal = 0;
	uint32 VictimId = 0;
	int32 RuleVersion = 0;
};

USTRUCT(BlueprintType)
struct FVehicleModuleSavedInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	int32 ModuleId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	EVehicleModuleTypes Type = EVehicleModuleTypes::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	float HealthFraction = 1.f;
};

// Add-on coverage is saved by armor-mesh registration slot and plate type, not by component pointer.
USTRUCT(BlueprintType)
struct FVehicleModuleSavedCoverage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	int32 ModuleId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	int32 ArmorMeshSlot = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	EArmorPlate PlateType = EArmorPlate::Plate_Front;
};

USTRUCT(BlueprintType)
struct FVehicleModuleSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	int32 RuleVersion = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	EVehicleModuleProfile Profile = EVehicleModuleProfile::MediumTank;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	EVehicleRunningGear RunningGear = EVehicleRunningGear::Tracks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	float FinishingWork = 0.f;

	// Serial used by impacts without weapon-supplied shot identity; keeps their rolls deterministic after load.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	int32 DefaultImpactSerial = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	TArray<FVehicleModuleSavedInstance> Modules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Vehicle Modules")
	TArray<FVehicleModuleSavedCoverage> Coverage;
};
