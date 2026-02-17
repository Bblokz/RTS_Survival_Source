// Copyright (C) 2020-2023 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/CPPWeaponsMaster.h"
#include <RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h>

#include "RTS_Survival/GameUI/Healthbar/AmmoHealthBar/AmmoHpBarTrackerState/FAmmoHpBarTrackerState.h"
#include "RTS_Survival/Weapons/WeaponAIState/WeaponAIState.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"
#include "RTS_Survival/Weapons/WeaponRangeData/WeaponRangeData.h"
#include "RTS_Survival/Weapons/WeaponTargetingData/WeaponTargetingData.h"

#include "CPPTurretsMaster.generated.h"

struct FAmmoTrackerInitSettings;
class ASmallArmsProjectileManager;
enum class ETargetPreference : uint8;
struct FInitWeaponStateProjectile;
struct FInitWeaponStateDirectHit;
struct FInitWeaponStateProjectile;
class ITurretOwner;
// Forward Declaration.
class RTS_SURVIVAL_API ATankMaster;

/** The type of idle rotation the turret uses. */
UENUM(BlueprintType)
enum EIdleRotation
{
	Idle_KeepLast,
	Idle_Base,
	Idle_Animate
};

USTRUCT()
struct FTurretIdleSelectState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsIdle = false;

	UPROPERTY()
	bool bIsSelected = false;
};

/**
 * @brief Idle/animation policy & local-yaw state for the turret.
 * Holds everything related to idle mode decisions, the per-idle local yaw target, and the animation timer.
 */
USTRUCT()
struct FTurretIdleAnimationState
{
	GENERATED_BODY()

	FTurretIdleAnimationState();

	/** "Forward" yaw when idle, captured from local (relative) rotation once. */
	UPROPERTY()
	float M_BaseLocalYaw;

	/** Local yaw target used while in Idle_Base. */
	UPROPERTY()
	float M_TargetLocalYaw ;

	/** When true, RotateTurret() drives relative yaw (tank space) instead of world yaw. */
	UPROPERTY()
	bool bM_UseLocalYawTarget = false;

	/** Which idle policy to apply (KeepLast / Base / Animate). */
	UPROPERTY()
	TEnumAsByte<EIdleRotation> M_IdleTurretRotationType;

	/** Timer handle for idle animation rotation updates. */
	FTimerHandle IdleAnimationTimerHandle;
};

/**
 * @brief Aiming/steering state for yaw: margin, delta, current desired rotation, last target pos & direction for weapons.
 * Used whenever we compute or chase a yaw target (world-steering).
 */
USTRUCT()
struct FTurretAimingSteeringState
{
	GENERATED_BODY()

	// The allowed difference in yaw to the target, if set to 0 then the rotation must be exactly in line to the targeted actor.
	UPROPERTY()
	float M_RotationMarge = 0.1f;

	// Difference in yaw between turret and target (for profiling/telemetry).
	UPROPERTY()
	double M_DeltaYaw = 0.0;

	// The target rotator the turret needs to align to (world).
	UPROPERTY()
	FRotator M_TargetRotator = FRotator::ZeroRotator;

	// The previous position of the target (world).
	UPROPERTY()
	FVector M_LastTargetPosition = FVector::ZeroVector;

	// Used by implemented weapons to receive any yaw offset (fire direction vector).
	UPROPERTY()
	FVector M_TargetDirection = FVector::ZeroVector;
};


/**
 * @brief Base turret controller owning weapon states, target selection, and steering. 
 * Turret can be driven in world-space (targets) or in tank local space (Idle_Base).
 * @note InitTurretsMaster: call in blueprint to set the skeletal mesh pointer before use.
 * @note InitChildTurret: call in blueprint to set rotation/pitch limits and idle policy.
 * @note SetupWeaponAmmoTracker: call in blueprint (post init) to enable ammo UI tracking.
 */
UCLASS()
class RTS_SURVIVAL_API ACPPTurretsMaster : public ACPPWeaponsMaster, public IWeaponOwner
{
	GENERATED_BODY()

	// to affect rotation speed.
	friend class RTS_SURVIVAL_API UTurretRotationBehaviour;
	friend class AEmbeddedTurretsMaster;
	friend class RTS_SURVIVAL_API UTankAimAbilityComponent;

	virtual void Tick(float DeltaTime) override;

public:
	// Public c-tor needed for call hierarchy with derived classes.
	ACPPTurretsMaster();

	/**
	 * @brief Sets this turret to automatically engage targets in range.
	 * To calculate whether the target is in range the first weapon in the weapon array of the turret is
	 * used to determine possible range of engagement.
	 * @param bUseLastTarget Whether to use the last targeted actor in the first autoEngage loop.
	 * @note Target is switched when it is out of range.
	 */
	UFUNCTION(BlueprintCallable)
	void SetAutoEngageTargets(const bool bUseLastTarget);

	/**
	 * @brief Sets this turret to engage the specific target, will not switch targets even when out of range.
	 * But will switch to AutoEngage if the target is destroyed.
	 * @param Target The target to engage.
	*/
	UFUNCTION(BlueprintCallable)
	void SetEngageSpecificTarget(AActor* Target);

	UFUNCTION(BlueprintCallable)
	void SetEngageGroundLocation(const FVector& GroundLocation);
	
	virtual void RegisterIgnoreActor(AActor* ActorToIgnore, const bool bRegister) override;

	inline bool GetIsRotatedToEngage() const { return bM_IsRotatedToEngage; }

	virtual TArray<UWeaponState*> GetWeapons() override final;
	int32 GetOwningPlayer();

	// Looks if the provided weapon is part of this turret, if so it upgrades the range on the weapon and
	// adjusts the search range of this turret accordingly.
	void UpgradeWeaponRange(const UWeaponState* MyWeapon, const float RangeMlt);

	// Disables the search handle nd stops all weapon fire.
	// Can be re-enabled by calling SetAutoEngageTargets or SetEngageSpecificTarget.
	void DisableTurret();

	void OnSetupTurret(AActor* OwnerOfTurret);

	float GetTurretRotationSpeed() const { return RotationSpeed; }
	void SetTurretRotationSpeed(const float NewRotationSpeed) { RotationSpeed = NewRotationSpeed; }

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_PostInit();

	// For embedded turret for struct interface.
	FVector& GetActiveTargetLocation() { return M_TargetingData.GetActiveTargetLocation(); }
	// For embedded turret for struct interface.
	bool HasValidTarget() const { return M_TargetingData.GetIsTargetValid(); }

	/** @return The owning player for this turret to setup the weapon collisions.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "InitWeaponOwner")
	int32 GetOwningPlayerForWeaponInit();

	// Determines for which type of actor target the turret will filter first.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Target")
	ETargetPreference TargetPreference;

	/**
	 * @brief Sets the PlayerController in RTS_BP_TurretMaster, this is a separate function from SetTurretOwner
	 * is only used to determine the actor that has mounted the turret.
	 * @param NewSceneSkeletalMesh The skeletal mesh of the turret.
	 * @post The turret is now able to properly initialise weapons.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ReferenceCasts")
	void InitTurretsMaster(USkeletalMeshComponent* NewSceneSkeletalMesh);

	/**
	 * @brief Init the values on a derived blueprint turret child.
	 * Make sure to: Override UpdateTargetPitch in the grandchild to control animation pitch and add a call to the parent event!
	 * Make sure to: Override the UpdateWeaponVectors in the grandChild for location and weapon direction.
	 * @param NewRotationSpeed How fast the turret turns in degrees / second.
	 * @param NewMaxPitch Maximal pitch in degrees.
	 * @param NewMinPitch Minimal pitch in degrees.
	 * @param NewIdleTurretRotationType The type of idle rotation the turret uses.
     * @note -----------------------------------------------------------------------------------------------
     * @note Only call on child blueprints not the master blueprint!
	 * @note -----------------------------------------------------------------------------------------------
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ReferenceCasts")
	void InitChildTurret(const float NewRotationSpeed,
	                     const float NewMaxPitch,
	                     const float NewMinPitch,
	                     EIdleRotation NewIdleTurretRotationType);

	virtual void OnWeaponAdded(const int32 WeaponIndex, UWeaponState* Weapon) override;
	virtual void OnWeaponBehaviourChangesRange(const UWeaponState* ReportingWeaponState, const float NewRange) override;

	// Call this in the PostInit to provide what type of tracking we want and on which weapon index.
	// The in post init the turret set the owner (on which the ammobar is placed) next to this data
	// Then on bp when the right weapon index is added we init the widget with the tracking settings.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="PostInitialization")
	void SetupWeaponAmmoTracker(FAmmoTrackerInitSettings AmmoTrackingSettings);

	/**
	 * @param WeaponIndex The index of the weapon that is firing.
	 * @return The targetpitch the weapon currently has towards the target used for trace fire.
	 * @note IWeaponOwner implementation; note also that both regular and embedded turrets use the same
	 * TargetPitch variable and therefore this implementation is final.
	 */
	virtual FVector& GetFireDirection(const int32 WeaponIndex) override final;
	virtual FVector& GetTargetLocation(const int32 WeaponIndex) override final;

	virtual bool AllowWeaponToReload(const int32 WeaponIndex) const override;

	/**
	 * Called by implemented weapon to check if the owner's target was destroyed.
	 * @param WeaponIndex The weapon that killed the actor.
	 * @param KilledActor The actor killed
	 * @note IWeaponOwner implementation; same for embedded turrets.
	 */
	virtual void OnWeaponKilledActor(const int32 WeaponIndex, AActor* KilledActor) override final;

	virtual void PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode) override;

	virtual void OnProjectileHit(const bool bBounced) override;

	// Play the animation for the weapon at this index; make sure to take into account the weapon fire mode!
	UFUNCTION(BlueprintImplementableEvent)
	void BP_PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode);

	/**
	 * Called when the weapon start reloading.
	 * @param WeaponIndex The index of the weapon that is reloading.
	 * @param ReloadTime How long the reload will take.
	 * @note IweaponOwner implementation.
	 */
	virtual void OnReloadStart(const int32 WeaponIndex, const float ReloadTime) override final;

	/** @param WeaponIndex The index of the weapon that Finished reloading.
	 * @note IWeaponOwner Implementation.*/
	virtual void OnReloadFinished(const int32 WeaponIndex) override;
	virtual void ForceSetAllWeaponsFullyReloaded() override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiTraceWeapon(FInitWeaponStateMultiTrace MultiTraceWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupLaserWeapon(const FInitWeaponStateLaser& LaserWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiHitLaserWeapon(const FInitWeaponStateMultiHitLaser& LaserWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupRocketProjectileWeapon(FInitWeaponStateRocketProjectile RocketProjectileParameters) override;
	virtual void SetupVerticalRocketProjectileWeapon(FInitWeaponStateVerticalRocketProjectile VerticalRocketProjectileParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileState) override;

        UFUNCTION(BlueprintCallable)
        virtual void SetupArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters) override;

        UFUNCTION(BlueprintCallable)
        virtual void SetupPooledArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters) override;

        UPROPERTY(blueprintReadOnly, VisibleDefaultsOnly)
        TScriptInterface<ITurretOwner> TurretOwner;

	virtual FRotator GetTargetRotation(const FVector& TargetLocation, const FVector& TurretLocation) const;

	virtual FTransform GetTurretTransform() const;

	// Reference to the SkeletalMesh of the turret.
	UPROPERTY(BlueprintReadOnly, Category = "Reference")
	USkeletalMeshComponent* SceneSkeletalMesh;

	// How fast the turret turns in degrees / second. 
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	float RotationSpeed;

	/**
	 * @brief Called on Bp to activate reload animations. After the specified time the reload is done.
	 * @param WeaponIndexOnTurret Index to identify which weapon must be reloaded
	 * @param ReloadTime time it takes to reload the weapon
	*/
	UFUNCTION(BlueprintImplementableEvent)
	void ReloadWeapon(const int WeaponIndexOnTurret, const float ReloadTime);

	/** @brief Determines how the turret weapon target pitch is updated, by default calls the UpdateTargetPitch on
	 * the blueprint instance. Is overwritten in embedded turrets to use the interface.
	 */
	virtual void UpdateTargetPitchCPP(float NewPitch);

	/**
	 * @brief Propagates the pitch from base turret position to the current target.
	 * @param NewPitch Pitch in degrees.
	 * @pre Assumes that the pitch is already clamped to acceptable values for animation.
	 * @note used to animate the gun depression / elevation.
	 * @note DO NOT CALL THIS FUNCTION DIRECTLY, USE UpdateTargetPitchCPP INSTEAD.
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void UpdateTargetPitchForAnimation(const float NewPitch);

	// The needed pitch for the gun elevation / depression.
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	float TargetPitch;

	// How many degrees the turret is allowed to be off target and still start engaging.
	float AllowedDegreesOffTarget = 4.f;

	/**
	 * @brief Initialises the turret owner interface pointer by looking for the actor that implements this turret
	 * as a child actor.
	 * @param NewTurretOwner The actor that owns this turret.
	 * @note -------------------------------------------------
	 * @note Called at beginplay of CPPTurretsMaster!
	 * @note -------------------------------------------------
	 * @note Virtual so the embedded Turret is able to setup
	 * @note the extra cached pointer to the parent actor
	 * @note as a IEmbeddedTurretInterface as well.
	 * @note -------------------------------------------------
	 */
	virtual void InitTurretOwner(AActor* NewTurretOwner);

	// Set to true whenever we recalculate the targetrotator.
	UPROPERTY()
	bool bIsTargetRotatorUpdated;

	UPROPERTY()
	FTurretIdleAnimationState IdleAnimationState;

	UPROPERTY()
	FTurretAimingSteeringState SteeringState;
	
	

private:
	UPROPERTY()
	TArray<UWeaponState*> M_TWeapons;

	UPROPERTY()
	UWorld* M_WorldSpawnedIn;

	// Whether the weapon is set to auto or specific engage.
	EWeaponAIState M_WeaponAIState;

	/** Unified targeting core for this weapon owner. */
	UPROPERTY()
	FWeaponTargetingData M_TargetingData;

	bool EnsureWorldIsValid();
	void SetOwningPlayer(const int32 NewOwningPlayer);

	/**
	 * @brief Engages timers for this turret to automatically search for targets to engage.
	 * @pre Assumes that weapon fire timers are stopped and turret is ready for a new target.
	 */
	void InitiateAutoEngageTimers();

	/**
	 * @brief Enables the timers of this turret to engage the specific target, will not switch targets even when out of range.
	 * But will switch to AutoEngage if the target is destroyed.
	 * @pre Assumes that weapon fire timers are stopped and turret is ready for a new target.
	*/
	void InitiateSpecificTargetTimers();

	/** @brief Automatically engages enemies within range of the first weapon on this turret. */
	UFUNCTION()
	void AutoEngage();

	/** @brief Tries to engage set target for as long as it is alive. */
	UFUNCTION()
	void SpecificEngage();

	// Will revert back to auto engage.
	void SpecificEngage_OnTargetInvalid();

	/**
	 * @brief Turns the turret towards the given actor (now unused actor) by using active target location.
	 * @param Target Unused (kept to preserve call sites); rotation is computed from cached target location.
	 */
	void RotateTurretToActor(const AActor* const Target);

	/**
	 * @brief Dispatch steering per mode: local idle-steering vs world-steering.
	 */
	virtual void RotateTurret(const float DeltaTime);

	/**
	 * @brief Smoothly rotate in tank local space towards fixed idle base yaw.
	 * @param DeltaTime Frame delta
	 */
	void RotateTurret_LocalIdle(const float DeltaTime);

	/**
	 * @brief Original world-space steering for targets/Idle_Animate, unchanged in behavior.
	 * @param DeltaTime Frame delta
	 */
	void RotateTurret_LocalYaw_Root(const float DeltaTime);

	// Stops the turret rotation timer.
	void StopTurretRotation();

	/** Set local yaw idle target and mark rotation as pending. */
	void SetIdleBaseLocalTarget();

	/** Per-pointer validator; no logging to avoid noise before Init. */
	bool GetIsValidSceneSkeletalMesh() const;

	FTimerHandle M_WeaponSearchHandle;
	FTimerDelegate M_WeaponSearchDel;

	inline static constexpr double M_WeaponSearchTimeInterval = 0.3;

	float M_MaxTurretPitch;
	float M_MinTurretPitch;

	FRotator GetBaseRotation() const;

	/** @brief Resets target logic for new target. */
	void ResetTarget();

	/** @brief Sets target logic for new target. */
	void SetTarget(AActor* NewTarget);

	/**
	* @brief Stops all weapon fire, initiates Turret for next shot.
	* @param InvalidateTarget Whether the current target should be invalidated and the search for a new target should begin
	*/
	void StopAllWeaponsFire(bool InvalidateTarget = false);

	void DisableAllWeapons();

	/** @brief sets ActorTarget to the closest enemy actor that is in range of the first weapon on this turret.*/
	UFUNCTION()
	void GetClosestTarget();

	/** @brief if the turret is rotated towards the target within margin to engage */
	bool bM_IsRotatedToEngage = false;

	/** @brief Whether the turret is fully rotated to the target rotator. */
	bool bM_IsFullyRotatedToTarget = false;

	/**
	 * @brief Loops through all mounted weapons and calls fire on each weapon.
	 */
	UFUNCTION()
	void FireAllWeapons();

	// Goes through all mounted weapons to find the max turret range.
	void UpdateTurretRangeBasedOnWeapons();

	// Keeps track of range and squared range as well as the search radius for this weapon.
	UPROPERTY()
	FWeaponRangeData M_WeaponRangeData;

	void OnTargetsFound(const TArray<AActor*>& Targets);

	bool bM_IsPendingTargetSearch;

	/** @brief Registers the OnProjectileManagerLoaded callback to the ProjectileManager.
	 * This registers the reference on the weapon state if that weapon state is of Trace type.
	 */
	void SetupCallbackToProjectileManager();

	/** @brief Goes through the weapons, if it is a trace weapon we setup the reference to the projectile manager.*/
	void OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager);

	// Cached after callback when the manager is initialized; used to set the reference on added weapons.
	TWeakObjectPtr<ASmallArmsProjectileManager> M_ProjectileManager;

	// The player that owns this turret.
	int32 M_OwningPlayer = -1;

	/** Called on timer to update idle animation rotation. */
	void UpdateIdleAnimationRotation();

	/** Starts the idle animation timer if the turret is set to Idle_Animate. */
	void StartIdleAnimationTimer();

	/** Stops the idle animation timer. */
	void StopIdleAnimationTimer();

	FTurretIdleSelectState IdleSelectState;

	// When the turret is idle.
	void OnTurretIdle();
	// When the turret is not longer idle.
	void OnTurretActive();
	void OnTurretSelected();
	void OnTurretDeselected();
	void InitSelectionDelegatesOfOwner(AActor* OwnerOfTurret);



	FAmmoHpBarTrackerState M_AmmoTrackingState;
};
