// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/DamageReduction/DamageReduction.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponShellType/WeaponShellType.h"

#include "ShieldTypes.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UW_ShieldBar;

UENUM(BlueprintType)
enum class EShieldSpawnMode : uint8
{
	Immediately,
	OnActivation
};

UENUM(BlueprintType)
enum class EShieldBehaviour : uint8
{
	KillOnDestroyed,
	Recharge,
	OnlyRechargeOnActivation
};

UENUM(BlueprintType)
enum class EShieldState : uint8
{
	Inactive,
	Active,
	Recharging,
	Destroying
};

UENUM(BlueprintType)
enum class EShieldDeactivationReason : uint8
{
	TimedOut,
	OwnerDestroyed
};

UENUM(BlueprintType)
enum class EShieldDamageSource : uint8
{
	Projectile,
	Hitscan,
	AreaOfEffect,
	Shrapnel,
	Flamethrower,
	Mine
};

UENUM(BlueprintType)
enum class EShieldDamageResult : uint8
{
	NotHandled,
	Absorbed,
	PassThrough
};

USTRUCT(BlueprintType)
struct FShieldDamageRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	float BaseDamage = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	float RangeAdjustedArmorPenetration = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	EWeaponShellType ShellType = EWeaponShellType::Shell_None;

	UPROPERTY(BlueprintReadWrite)
	EShieldDamageSource DamageSource = EShieldDamageSource::Projectile;

	UPROPERTY(BlueprintReadWrite)
	FVector ImpactLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FShieldActivationOverrides
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideShieldMaterial = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideShieldMaterial"))
	TObjectPtr<UMaterialInterface> ShieldMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideRadius = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideRadius"))
	float Radius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideTimedLife = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideTimedLife"))
	float TimedLife = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideCurrentShields = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideCurrentShields"))
	float CurrentShields = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideMaxShields = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideMaxShields"))
	float MaxShields = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideShieldOffset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideShieldOffset"))
	FVector ShieldOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideShieldBehaviour = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideShieldBehaviour"))
	EShieldBehaviour ShieldBehaviour = EShieldBehaviour::KillOnDestroyed;
};

USTRUCT(BlueprintType)
struct FShieldComponentSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EShieldSpawnMode SpawnMode = EShieldSpawnMode::OnActivation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EShieldBehaviour ShieldBehaviour = EShieldBehaviour::KillOnDestroyed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MaxShields = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FDamageReductionSettings DamageReductionSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> ShieldSphereMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UMaterialInterface> ShieldMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TSubclassOf<UW_ShieldBar> ShieldBarWidgetClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float Radius = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TimedLife = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float RechargeDelay = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FVector ShieldOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FVector ShieldBarOffset = FVector(0.0f, 0.0f, 600.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 CooldownDuration = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 PreferredAbilityIndex = INDEX_NONE;
};
