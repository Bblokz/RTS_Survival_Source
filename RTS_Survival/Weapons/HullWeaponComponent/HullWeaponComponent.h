// Copyright (C) Bas Blokzijl - All rights reserved.


#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h"
#include "RTS_Survival/Weapons/WeaponRangeData/WeaponRangeData.h"
#include "RTS_Survival/Weapons/WeaponTargetingData/WeaponTargetingData.h"

#include "HullWeaponComponent.generated.h"


struct FWeaponRangeData;
enum class ETargetPreference : uint8;
class UGameUnitManager;
class ITurretOwner;
class UWeaponState;
class ASmallArmsProjectileManager;
enum class EWeaponAIState : uint8;


USTRUCT(BlueprintType)
struct FHullWeaponSettings
{
	GENERATED_BODY()

	// Defines the minimum and maximum Yaw the Hull weapon can have.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FVector2D MinMaxYaw = FVector2D(0.f, 0.f);

	// Defines the minimum and maximum pitch the Hull weapon can have.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FVector2D MinMaxPitch = FVector2D(0.f, 0.f);
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UHullWeaponComponent : public UActorComponent, public IWeaponOwner
{
	GENERATED_BODY()

public:
	UHullWeaponComponent();

	void SetOwningPlayer(const int32 PlayerIndex);


	/**
	 * @brief Sets this Hull weapon to automatically engage targets in range.
	 * To calculate whether the target is in range the first weapon in the weapon array of the HullWeapon is
	 * used to determine possible range of engagement.
	 * @param bUseLastTarget Whether to use the last targeted actor in the first autoEngage loop.
	 * @note Target is switched when it is out of range.
	 */
	UFUNCTION(BlueprintCallable)
	void SetAutoEngageTargets(const bool bUseLastTarget);

	/**
	 * @brief Sets this HullWeapon to engage the specific target, will not switch targets even when out of range.
	 * But will switch to AutoEngage if the target is destroyed.
	 * @param Target The target to engage.
	*/
	UFUNCTION(BlueprintCallable)
	void SetEngageSpecificTarget(AActor* Target);

	UFUNCTION(BlueprintCallable)
    void SetEngageGroundLocation(const FVector& GroundLocation);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void DisableHullWeapon();

	virtual TArray<UWeaponState*> GetWeapons() override final { return M_TWeapons; }

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitHullWeaponComponent(UMeshComponent* HullWeaponMesh, FHullWeaponSettings Settings);
	
	virtual void RegisterIgnoreActor(AActor* ActorToIgnore, const bool bRegister) override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// ---- Begin Override IWeaponOwner ----
	virtual void OnWeaponAdded(const int32 WeaponIndex, UWeaponState* Weapon) override;
	virtual FVector& GetFireDirection(const int32 WeaponIndex) override final;
	virtual bool AllowWeaponToReload(const int32 WeaponIndex) const override;
	virtual void OnWeaponKilledActor(const int32 WeaponIndex, AActor* KilledActor) override final;
	virtual void PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode) override final;
	virtual void OnProjectileHit(const bool bBounced) override;
	virtual void OnReloadStart(const int32 WeaponIndex, const float ReloadTime) override final;
	virtual void OnReloadFinished(const int32 WeaponIndex) override final;

	UFUNCTION(BlueprintCallable)
	virtual void SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiTraceWeapon(FInitWeaponStateMultiTrace MultiTraceWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupLaserWeapon(const FInitWeaponStateLaser& LaserWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileState) override;

        UFUNCTION(BlueprintCallable)
        virtual void SetupArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters) override;

        UFUNCTION(BlueprintCallable)
        virtual void SetupPooledArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters) override;

        // ---- End Override IWeaponOwner ----

	UPROPERTY(blueprintReadOnly, VisibleDefaultsOnly)
	TScriptInterface<ITurretOwner> TurretOwner;


	// Determines for which type of actor target the turret will filter first.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Target")
	ETargetPreference TargetPreference;

private:
	UPROPERTY()
	UWorld* M_WorldSpawnedIn;
	
	/** Unified targeting core for this weapon owner. */
	UPROPERTY()
	FWeaponTargetingData M_TargetingData;

	bool EnsureWorldIsValid();
	bool GetIsValidTurretOwner() const;

	void InitiateAutoEngageTimers();
	void InitiateSpecificEngageTimers();

	UPROPERTY()
	TArray<UWeaponState*> M_TWeapons;

	TWeakObjectPtr<UMeshComponent> M_HullWeaponMesh;

	bool GetIsValidHullWeaponMesh() const;


	void BeginPlay_InitTurretOwner(AActor* NewTurretOwner);

	bool GetIsValidWeapon(const UWeaponState* Weapon) const;
	void AllWeaponsStopFire(const bool bStopReload, const bool bStopCoolDown);
	void AllWeaponsFire();
	void DisableAllWeapons();


	int32 M_OwningPlayer;

	inline static constexpr double M_WeaponSearchTimeInterval = 0.3;

	FTimerDelegate M_WeaponSearchDel;
	FTimerHandle M_WeaponSearchHandle;

	void BeginPlay_SetupMeshOrientationTimer();
	FTimerHandle M_WeaponMeshOrientationHandle;
	void OnUpdateMeshOrientation();

	// Defines the degrees of freedom the hull weapon has to track the target.
	FHullWeaponSettings M_HullWeaponSettings;

	void AutoEngage();
	void SpecificEngage();
	void SpecificEngage_OnTargetInvalid();

	// Reset the target and stop weapon fire.
	void OnTargetInvalidOrOutOfReach();
	bool GetIsTargetInRange(const FVector& TargetLocation, const FVector& WeaponLocation) const;
	bool GetIsTargetWithinYaw() const;

	void GetClosestTarget();

	// Set the target to null when it is invalid, out of range or out of reach.
	// Also restores the target direction to base position.
	void ResetTarget();

	// Whether we are waiting for async target search.
	bool bM_IsPendingTargetSearch;

	void OnTargetsFound(const TArray<AActor*>& Targets);


	// Whether the weapon is set to auto or specific engage.
	EWeaponAIState M_WeaponAIState;

	// Used for weapon fire, needs to be in world space for proper fire direction.
	FVector M_TargetDirectionVectorWorldSpace;


	/** @brief Registers the OnProjectileManagerLoaded callback to the ProjectileManager.
	 * This registers the reference on the weapon state if that weapon state is of Trace type.
	 */
	void BeginPlay_SetupCallbackToProjectileManager();

	TWeakObjectPtr<ASmallArmsProjectileManager> M_ProjectileManager;
	void OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager);


	// Goes throuh all mounted weapons to find the max turret range.
	void UpdateHullWeaponRangeBasedOnWeapons();

	// Keeps track of range and squared range as well as the search readius for this weapon.
	UPROPERTY()
	FWeaponRangeData M_WeaponRangeData;

	// To register/ deregister the unit with the GameUnitManager.
	TWeakObjectPtr<UGameUnitManager> M_GameUnitManager;
	void BeginPlay_SetupGameUnitManager();
	bool GetIsValidGameUnitManager() const;
	bool GetIsValidOwner() const;

	// Initial relative rotation offset (usually identity when attached to a socket).
	UPROPERTY()
	FQuat M_BaseRelativeQuat = FQuat::Identity;

	// Cached desired world rotation built each tick.
	UPROPERTY()
	FQuat M_DesiredWorldQuat = FQuat::Identity;

	// True after first successful init
	bool bM_BaseInit = false;


	// Helpers
	double SignedAngleDegAroundAxis(const FVector& From, const FVector& To, const FVector& Axis) const;
	bool ComputeDesiredAim(const FVector& TargetWorld, FQuat& OutDesiredQuat, double& OutYawDeg,
	                       double& OutPitchDeg) const;
	bool GetParentSocketWorldTransform(FTransform& OutParentSocketWorld) const;


	static FVector SafePlaneProject(const FVector& V, const FVector& PlaneNormal);

	void Debug_HullWeapons(const FString& DebugMessage, const FColor& Color) const;
};
