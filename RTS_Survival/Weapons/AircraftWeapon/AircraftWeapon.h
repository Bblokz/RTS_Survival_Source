// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h"
#include "RTS_Survival/Weapons/WeaponRangeData/WeaponRangeData.h"
#include "RTS_Survival/Weapons/WeaponTargetingData/WeaponTargetingData.h"
#include "AircraftWeapon.generated.h"


class AAircraftMaster;
class ASmallArmsProjectileManager;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAircraftWeapon : public UActorComponent, public IWeaponOwner
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UAircraftWeapon();

	void InitAircraftWeaponComponent(
		AAircraftMaster* AircraftOwner,
		UMeshComponent* AircraftWeaponMesh);

	/** Setter mirroring turret/hull/infantry: keeps WeaponState owners and target struct in sync. */
	void SetOwningPlayer(const int32 PlayerIndex);

	void UpdateLandedState(const EAircraftLandingState NewLandedState)
	{
		M_LandedState = NewLandedState;
	}

	void ForceInstantReloadAllWeapons();

	void AllWeaponsStopFire(const bool bStopReload, const bool bStopCoolDown);
	void AllWeaponsFire(AActor* TargetActor);

	float GetTotalReloadTimeOfWeapons();

	virtual TArray<UWeaponState*> GetWeapons() override final { return M_TWeapons; }

	virtual void RegisterIgnoreActor(AActor* ActorToIgnore, const bool bRegister) override;
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Start I Weapon owner interface.
	virtual void OnWeaponAdded(const int32 WeaponIndex, UWeaponState* Weapon) override;
	virtual FVector& GetFireDirection(const int32 WeaponIndex) override;
	virtual FVector& GetTargetLocation(const int32 WeaponIndex) override;
	virtual bool AllowWeaponToReload(const int32 WeaponIndex) const override;
	virtual void OnWeaponKilledActor(const int32 WeaponIndex, AActor* KilledActor) override;
	virtual void PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode) override;
	virtual void OnReloadStart(const int32 WeaponIndex, const float ReloadTime) override;
	virtual void OnReloadFinished(const int32 WeaponIndex) override;
	virtual void OnProjectileHit(const bool bBounced) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters) override;
	UFUNCTION(BlueprintCallable)
	virtual void SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters) override;
	UFUNCTION(BlueprintCallable)
        virtual void SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters) override;
	UFUNCTION(BlueprintCallable)
	virtual void SetupRocketProjectileWeapon(FInitWeaponStateRocketProjectile RocketProjectileParameters) override;
	UFUNCTION(BlueprintCallable)
        virtual void SetupArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters) override;
        UFUNCTION(BlueprintCallable)
        virtual void SetupPooledArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters) override;
	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiTraceWeapon(FInitWeaponStateMultiTrace MultiTraceWeaponParameters) override;
	UFUNCTION(BlueprintCallable)
	virtual void SetupLaserWeapon(const FInitWeaponStateLaser& LaserWeaponParameters) override;
	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiHitLaserWeapon(const FInitWeaponStateMultiHitLaser& LaserWeaponParameters) override;
	virtual void SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters) override;
	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileState) override;

private:
	UPROPERTY()
	TArray<UWeaponState*> M_TWeapons;

	bool GetIsValidWeapon(const UWeaponState* Weapon) const;

	/** @brief Registers the OnProjectileManagerLoaded callback to the ProjectileManager.
	 * This registers the reference on the weapon state if that weapon state is of Trace type.
	 */
	void BeginPlay_SetupCallbackToProjectileManager();

	void OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager);

	// Goes throuh all mounted weapons to find the max turret range.
	void UpdateRangeBasedOnWeapons();

	// Keeps track of range and squared range as well as the search readius for this weapon.
	UPROPERTY()
	FWeaponRangeData M_WeaponRangeData;

	int32 M_OwningPlayer;

	/** Unified targeting for aircraft: Actor/Ground/None + aimpoint cycling support. */
	UPROPERTY()
	FWeaponTargetingData M_TargetingData;

	UPROPERTY()
	TMap<int32, FName> M_WeaponIndexToSocket;

	UPROPERTY()
	UWorld* M_WorldSpawnedIn;

	UPROPERTY()
	EAircraftLandingState M_LandedState;

	bool EnsureWorldIsValid();

	// The mesh on which the weapons are mounted, used to access the socket locations.
	TWeakObjectPtr<UMeshComponent> M_AircraftWeaponMesh;

	TWeakObjectPtr<AAircraftMaster> M_AircraftOwner;

	bool EnsureAircraftWeaponMeshIsValid() const;
	bool EnsureAircraftOwnerIsValid() const;

	// Set by callback at begin play when the manager is loaded.
	UPROPERTY()
	TWeakObjectPtr<ASmallArmsProjectileManager> M_ProjectileManager;

	bool GetIsProjectileManagerLoaded() const;

	FVector GetDirectionVectorToTarget(const FVector& TargetLocation, const FVector& StartLocation) const;

	FVector M_FireDir = {0.f, 0.f, 0.f};
};
