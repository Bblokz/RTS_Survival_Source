#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"

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


};
