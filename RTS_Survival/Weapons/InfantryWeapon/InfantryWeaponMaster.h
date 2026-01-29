// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/CPPWeaponsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h"
#include "RTS_Survival/Weapons/WeaponRangeData/WeaponRangeData.h"
#include "RTS_Survival/Weapons/WeaponTargetingData/WeaponTargetingData.h"
#include "InfantryWeaponMaster.generated.h"

class ASmallArmsProjectileManager;
enum class EWeaponAIState : uint8;
enum class ETargetPreference : uint8;
enum class ESquadWeaponAimOffset : uint8;
class USquadUnitAnimInstance;
class UGameUnitManager;
class ASquadUnit;
class ACPPController;

/**
 * @brief Infantry-carried weapon owner that manages one weapon state and target acquisition/firing.
 * Uses the shared FWeaponTargetingData for target/aim-point logic (Actor/Ground/None).
 * @note SetupOwner: call in blueprint to bind the owning squad unit & anim instance.
 */
UCLASS()
class RTS_SURVIVAL_API AInfantryWeaponMaster : public ACPPWeaponsMaster, public IWeaponOwner
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	AInfantryWeaponMaster();

	/** Set/refresh the owning player and seed the targeting struct. */
	void SetOwningPlayer(const int32 PlayerIndex);

	/**
	 * @brief Sets this weapon to automatically engage targets in range.
	 * To calculate whether the target is in range the first weapon in the weapon array of the InfantryWeapon is
	 * used to determine possible range of engagement.
	 * @param bUseLastTarget Whether to use the last targeted actor in the first autoEngage loop.
	 * @note Target is switched when it is out of range.
	 */
	UFUNCTION(BlueprintCallable)
	void SetAutoEngageTargets(const bool bUseLastTarget);

	/**
	 * @brief Sets this InfantryWeapon to engage the specific target, will not switch targets even when out of range.
	 * But will switch to AutoEngage if the target is destroyed.
	 * @param Target The target to engage.
	*/
	UFUNCTION(BlueprintCallable)
	void SetEngageSpecificTarget(AActor* Target);

	
	UFUNCTION(BlueprintCallable)
    void SetEngageGroundLocation(const FVector& GroundLocation);

	/**
	 * @brief Checks if the provided actor is the targeted actor by this turret.
	 * if so then the turret owner determines if this turret will autoengage or specifically engage another target.
	 * @param KilledActor The actor killed by a weapon of this turret.
	 */
	void CheckTargetKilled(AActor* KilledActor);

	virtual TArray<UWeaponState*> GetWeapons() override final;

	// Stops any weapon fire and searching, can be turned on again by SetAutoEngageTargets.
	/** @param bStopReload Whether to stop the reload of the weapon. */
	void DisableWeaponSearch(const bool bStopReload = false, const bool bMakeWeaponInvisible = false);

	void DisableAllWeapons();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupOwner(AActor* InActor);

	inline ESquadWeaponAimOffset GetAimOffsetType() const { return M_AimOffsetType; }

	/**
	 * @return The mesh of this weapon, needed by a secondary weapon component to set the mesh on the back of the
	 * unit when switching weapons.
	 */
	UStaticMesh* GetWeaponMesh() const { return IsValid(WeaponMesh) ? WeaponMesh->GetStaticMesh() : nullptr; }

	virtual void RegisterIgnoreActor(AActor* ActorToIgnore, const bool bRegister) override;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category="Weapon")
	float TestAngle;

	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostInitializeComponents() override;

	// Setup weapon aim offset in constructor
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Constructor")
	void SetWeaponAimOffsetType(ESquadWeaponAimOffset AimOffsetType);

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	int32 GetOwningPLayerForWeaponInit();

	UPROPERTY(BlueprintReadWrite)
	UStaticMeshComponent* WeaponMesh;

	// Determines for which type of actor target the turret will filter first.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Target")
	ETargetPreference TargetPreference;

	// ---- IWeaponOwner ----
	virtual void OnWeaponAdded(const int32 WeaponIndex, UWeaponState* Weapon) override;
	virtual void OnWeaponBehaviourChangesRange(const UWeaponState* WeaponState, const float NewRange) override;
	virtual FVector& GetFireDirection(const int32 WeaponIndex) override;
	virtual FVector& GetTargetLocation(const int32 WeaponIndex) override;
	virtual bool AllowWeaponToReload(const int32 WeaponIndex) const override;
	virtual void OnWeaponKilledActor(const int32 WeaponIndex, AActor* KilledActor) override;
	virtual void PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode) override;
	virtual void OnProjectileHit(const bool bBounced) override;
	virtual void OnReloadStart(const int32 WeaponIndex, const float ReloadTime) override;
	virtual void OnReloadFinished(const int32 WeaponIndex) override;
	virtual void ForceSetAllWeaponsFullyReloaded() override;
	// ---- end IWeaponOwner ----

	UFUNCTION(BlueprintCallable)
	virtual void SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters) override;
	
        UFUNCTION(BlueprintCallable)
        virtual void SetupRocketProjectileWeapon(FInitWeaponStateRocketProjectile RocketProjectileParameters) override;

        UFUNCTION(BlueprintCallable)
        virtual void SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileState) override;

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
	
	UFUNCTION(BlueprintCallable)
	virtual void SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters) override;

private:
	/** @return Whether the owner squad unit of this weapon is valid, if not attempts to repair the reference. */
	bool GetIsSquadUnitOwnerValid();
	void ResetTarget();
	void SetupRange();

	
	/** Given the current angle to the target set the correct aim offset.
	* @return Whether the weapon is allowed to fire.
	*/ 
	bool GetIsTargetWithinAngleLimitsAndSetAO(const bool bInvalidateTargetOutOfReach);

	/**
	 * @brief Sets the AO angle for the idle animation state.
	 * @return Whether the current animation state allows the weapon to fire.
	 */
	bool SetAO_Idle(const float AngleToTarget) const;

	/**
	 * @brief Sets the AO angle for the walking animation state. 
	 * @return Whether the current animation state allows the weapon to fire.
	 */
	bool SetAO_Walking(const float AngleToTarget) const;

	/** Compute look direction towards the struct’s active target location. */
	FVector CalculateTargetDirection(const FVector& WeaponLocation);

	void InitiateAutoEngageTimers();
	void InitiateSpecificEngageTimers();

	/** @brief Registers the OnProjectileManagerLoaded callback to the ProjectileManager.
	 * This registers the reference on the weapon state if that weapon state is of Trace type.
	 */
	void BeginPlay_SetupCallbackToProjectileManager();

	/** Init the GameUnitManager reference. */
	void BeginPlay_SetupGameUnitManager();

	/** Validate the GameUnitManager. */
	bool GetIsValidGameUnitManager() const;
	bool GetIsValidWeaponState() const;

	UPROPERTY()
	TObjectPtr<UWeaponState> WeaponState;

	UPROPERTY()
	UWorld* M_WorldSpawnedIn;

	FTimerDelegate M_WeaponSearchDel;
	FTimerHandle M_WeaponSearchHandle;

	void AutoEngage();
	void SpecificEngage();

	inline static constexpr double M_WeaponSearchTimeInterval = 0.3;

	UPROPERTY()
	ASquadUnit* M_OwningSquadUnit;

	// Keeps track of range and squared range as well as the search radius for this weapon.
	UPROPERTY()
	FWeaponRangeData M_WeaponRangeData;

	// Used by the animation bp of the squad unit to determine what aim offset to use for this weapon.
	UPROPERTY()
	ESquadWeaponAimOffset M_AimOffsetType;

	UPROPERTY()
	TObjectPtr<USquadUnitAnimInstance> M_SquadUnitAnimInstance;

	bool IsSquadUnitAnimInstanceValid();
	void CheckIfMeshIsValid();

	void GetClosestTarget();
	inline bool GetIsTargetValidVisibleAndInRange();

	FVector M_FireDirection;

	int32 M_OwningPlayer;

	// Whether the weapon is waiting for a target from the async GetClosestTarget function.
	bool bM_IsPendingTarget;

	void OnTargetsFound(const TArray<AActor*>& Targets);

	void Debug_Weapons(const FString& DebugMessage) const;

	// Whether the weapon is set to auto or specific engage.
	EWeaponAIState M_WeaponAIState;

	// Cached after callback when the manager is initialized; used to set the reference on added weapons.
	TWeakObjectPtr<ASmallArmsProjectileManager> M_ProjectileManager;
	void OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager) const;

	// Manager to fetch closest targets (previously used but not stored in header).
	TWeakObjectPtr<UGameUnitManager> M_GameUnitManager;

	/** Unified targeting core for this infantry weapon (Actor/Ground/None + 4-point aim). */
	UPROPERTY()
	FWeaponTargetingData M_TargetingData;
};
