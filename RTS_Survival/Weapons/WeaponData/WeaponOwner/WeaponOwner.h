// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/FlameThrowerWeapon/FlameThrowerInit.h"
#include "RTS_Survival/Weapons/WeaponData/MultiProjectileWeapon/UWeaponStateMultiProjectile.h"
#include "UObject/Interface.h"
#include "WeaponOwner.generated.h"

struct FInitWeaponStateLaser;
struct FInitWeaponStateMultiHitLaser;
struct FInitWeaponStateMultiTrace;
struct FInitWeaponStateArchProjectile;
struct FInitWeaponStateSplitterArchProjectile;
struct FInitWeaponStateRocketProjectile;
struct FInitWeaponStateVerticalRocketProjectile;
struct FWeaponData;
class ASquadController;
class UWeaponState;
struct FInitWeaponStateProjectile;
struct FInitWeaponStateDirectHit;
enum class EWeaponFireMode : uint8;
struct FInitWeaponStatTrace;
// This class does not need to be modified.
UINTERFACE(MinimalAPI, NotBlueprintable)
class UWeaponOwner : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class RTS_SURVIVAL_API IWeaponOwner
{
	GENERATED_BODY()
	

	// To not bloat public interface with functions that are only used by the weapon system, we put them in protected.
	friend class RTS_SURVIVAL_API UWeaponState;
	friend class RTS_SURVIVAL_API UWeaponStateDirectHit;
	friend class RTS_SURVIVAL_API UWeaponStateTrace;
	friend class RTS_SURVIVAL_API UWeaponStateMultiTrace;
	friend class RTS_SURVIVAL_API UWeaponStateProjectile;
	friend class RTS_SURVIVAL_API UWeaponStateMultiProjectile;
	friend class RTS_SURVIVAL_API UWeaponStateArchProjectile;
	friend class RTS_SURVIVAL_API UWeaponStateSplitterArchProjectile;
	friend class RTS_SURVIVAL_API UWeaponStateRocketProjectile;
	friend class RTS_SURVIVAL_API UVerticalRocketWeaponState;
	friend class RTS_SURVIVAL_API UWeaponStateLaser;
	friend class RTS_SURVIVAL_API UWeaponStateMultiHitLaser;
	/** @return The array of weapons that this owner has. */
	virtual TArray<UWeaponState*> GetWeapons() =0;
	// To register or deregister actors to ignore with this weapon.
	virtual void RegisterIgnoreActor(AActor* ActorToIgnore, const bool bRegister) = 0;	
	/** Force every weapon owned by this implementor to be instantly reloaded. */
	virtual void ForceSetAllWeaponsFullyReloaded() = 0;
	

protected:
	// Called by the weapon state on the owner once it is initialized.
	virtual void OnWeaponAdded(const int32 WeaponIndex, UWeaponState* Weapon) = 0;
	/**
	 * @brief Notify owners to refresh cached ranges after behaviour driven range changes.
	 * @param ReportingWeaponState The weapon whose range changed.
	 * @param NewRange The updated range after behaviour adjustments.
	 */
	virtual void OnWeaponBehaviourChangesRange(const UWeaponState* ReportingWeaponState, const float NewRange) = 0;
	/**
	 * @param WeaponIndex The index of the weapon that is firing.
	 * @return The targetpitch the weapon currently has towards the target used for trace fire.
	 */
	virtual FVector& GetFireDirection(const int32 WeaponIndex) = 0;

	virtual FVector& GetTargetLocation(const int32 WeaponIndex) = 0;

	// Can be set to return false if you want to manually order when weapons will be reloaded.
	virtual bool AllowWeaponToReload(const int32 WeaponIndex) const = 0;


	/**
	 * Called by implemented weapon to check if the owner's target was destroyed.
	 * @param WeaponIndex The weapon that killed the actor.
	 * @param KilledActor The actor killed
	 */
	virtual void OnWeaponKilledActor(const int32 WeaponIndex, AActor* KilledActor) = 0;

	// Called when the weapon fires.
	virtual void PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode) = 0;

	/**
	 * Called when the weapon start reloading.
	 * @param WeaponIndex The index of the weapon that is reloading.
	 * @param ReloadTime How long the reload will take.
	 */
	virtual void OnReloadStart(const int32 WeaponIndex, const float ReloadTime) = 0;

	virtual void OnProjectileHit(const bool bBounced) = 0; 

	/** @param WeaponIndex The index of the weapon that Finished reloading. */
	virtual void OnReloadFinished(const int32 WeaponIndex) = 0;

	UFUNCTION(BlueprintCallable)
	virtual void SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters) = 0;

	UFUNCTION(BlueprintCallable)
	virtual void SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters) = 0;

	UFUNCTION(BlueprintCallable)
	virtual void SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters) = 0;

	UFUNCTION(BlueprintCallable)
	virtual void SetupRocketProjectileWeapon(FInitWeaponStateRocketProjectile RocketProjectileParameters) = 0;

	UFUNCTION(BlueprintCallable)
	virtual void SetupVerticalRocketProjectileWeapon(FInitWeaponStateVerticalRocketProjectile VerticalRocketProjectileParameters) = 0;
	
	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileState)=0;

        UFUNCTION(BlueprintCallable)
        virtual void SetupArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters) =0;

        UFUNCTION(BlueprintCallable)
        virtual void SetupPooledArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters) = 0;

	UFUNCTION(BlueprintCallable)
	virtual void SetupSplitterArchProjectileWeapon(FInitWeaponStateSplitterArchProjectile SplitterArchProjParameters) = 0;

	/** @brief Setup a weapon that fires async traces from multiple sockets in parallel. */
	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiTraceWeapon(FInitWeaponStateMultiTrace MultiTraceWeaponParameters) = 0;
	
	UFUNCTION(BlueprintCallable)
	virtual void SetupLaserWeapon(const FInitWeaponStateLaser& LaserWeaponParameters) = 0;

	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiHitLaserWeapon(const FInitWeaponStateMultiHitLaser& LaserWeaponParameters) = 0;

	UFUNCTION(BlueprintCallable)
    virtual void SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters) = 0;
	

};
