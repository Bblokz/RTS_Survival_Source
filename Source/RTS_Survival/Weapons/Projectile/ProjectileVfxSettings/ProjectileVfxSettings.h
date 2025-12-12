#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponShellType/WeaponShellType.h"

#include "ProjectileVfxSettings.generated.h"

UENUM(BlueprintType)
enum class EProjectileNiagaraSystem : uint8
{
	None,
	TankShell,
	AAShell,
	Bazooka,
	AttachedRocket,
};

enum class EWeaponShellType : uint8;

USTRUCT(BlueprintType)
struct FProjectileVfxSettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EWeaponShellType ShellType = EWeaponShellType::Shell_AP;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float WeaponCaliber = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EProjectileNiagaraSystem ProjectileNiagaraSystem = EProjectileNiagaraSystem::None;
};

