// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BarrageProjectileMover.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/Weapons/Projectile/ProjectileVfxSettings/ProjectileVfxSettings.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "GA_Barrage.generated.h"

class ASmallArmsProjectileManager;
class USoundBase;

USTRUCT(BlueprintType)
struct FBarrageSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinHeightStart = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHeightStart = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AmountShells = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BurstAmount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinTimeBetweenBurst = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxTimeBetweenBurst = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bVariationWeaponCalibre = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OnVariationMinMlt = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OnVariationMaxMlt = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FProjectileVfxSettings ProjectileVfxSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseOffMapLaunchSound = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundBase> OffMapLaunchSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeBetweenOffMapSoundAndStart = 0.0f;
};

/**
 * @brief Template ability used by designers to fire pooled arcing shells at a target area.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_Barrage : public UGlobalAbility
{
	GENERATED_BODY()

public:
	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation) override;

	virtual void BeginDestroy() override;
	
protected:
	virtual void OnInit(AActor* WorldContextActor) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FBarrageSettings M_BarrageSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FWeaponData M_WeaponData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FWeaponVFX M_WeaponVfx;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FBarrageProjectileMover M_ProjectileMover;

	UPROPERTY(Transient)
	TWeakObjectPtr<ASmallArmsProjectileManager> M_ProjectileManager;

	UPROPERTY(Transient)
	FVector M_PendingTargetLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	int32 M_ShellsLaunched = 0;

	UPROPERTY(Transient)
	FTimerHandle M_BurstTimerHandle;

	UPROPERTY(Transient)
	TArray<FTimerHandle> M_ShellLaunchTimerHandles;

	UPROPERTY(Transient)
	bool bM_IsWaitingForProjectileManager = false;

	void SetupCallbackToProjectileManager(const AActor* WorldContextObject);
	void OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager);
	bool GetIsValidProjectileManager() const;
	void StartBarrage(const FVector& TargetLocation);
	void FireNextBurst();
	void FireSingleShell();
	void LaunchSingleShell();
	bool GetShouldDelayShellLaunchForOffMapSound() const;
	void ScheduleDelayedShellLaunch();
	FWeaponVFX BuildWeaponVfxForShellLaunch() const;
	void PlayOffMapLaunchSoundIfRequested() const;
	USoundBase* GetOffMapLaunchSound() const;
	FVector BuildRandomPointInRadius(const FVector& CenterLocation) const;
	FVector BuildLaunchLocation(const FVector& TargetLocation) const;
	FWeaponData BuildVariedWeaponData() const;
	void ScheduleNextBurst();
	void ClearBurstTimer();
	void ClearShellLaunchTimers();
	void ResetBarrageRuntimeState();
	bool HasCachedProjectileManager() const;
};
