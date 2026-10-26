#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Subsystems/FireSubsystem/GroundFire/RTSGroundFireSettings.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponShellType/WeaponShellType.h"

#include "AttachedRocketsData.generated.h"

USTRUCT(BlueprintType)
struct FAttachedRocketsData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERTSDamageType DamageType = ERTSDamageType::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Damage = 50.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 DamageFluxMlt = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ArmorPenFluxMlt = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Range= 8000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorPen = 50.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ReloadSpeed = 30.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Cooldown = 4.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CoolDownFluxMlt = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProjectileSpeed = 6000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProjectileAccelerationFactor = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Accuracy = 80;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 WeaponCalibre = 30;

	// Shell type the rocket projectile flies with (impact effects, stun rules); the stats above are used as they are.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWeaponShellType ShellType = EWeaponShellType::Shell_HEAT;

	// Lingering burning ground left where each rocket explodes; off by default.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRTSGroundFireSettings GroundFire;

	// One target is picked per salvo (see HomingConeHalfAngleDegrees) and every rocket of the salvo homes onto it;
	// without a target the rockets fly straight as usual.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Homing")
	bool bHomingMissiles = false;

	// Only the homing path, speed and turn settings are used; RocketSettings inside is ignored.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Homing", meta=(EditCondition="bHomingMissiles"))
	FHomingMissileWeaponSettings HomingSettings;

	// Targets must be within this angle of the vehicle's heading (and within Range): first the owner's current
	// weapon target, else the closest enemy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Homing", meta=(EditCondition="bHomingMissiles",
		ClampMin="0.0", ClampMax="180.0"))
	float HomingConeHalfAngleDegrees = 60.f;
};
