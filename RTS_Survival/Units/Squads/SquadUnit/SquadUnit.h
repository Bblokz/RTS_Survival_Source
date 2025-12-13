// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpCharacterObjectsMaster.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceInterface/ExperienceInterface.h"
#include "RTS_Survival/RTSComponents/NavCollision/RTSNavCollision.h"
#include "RTS_Survival/Weapons/AimOffsetProvider/AimOffsetProvider.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "SquadUnit.generated.h"

class USquadUnitSpatialVoiceLinePlayer;
class UCargo;
enum class ERTSNavAgents : uint8;
class URTSSquadUnitOptimizer;
class URepairComponent;
class UFowComp;
class AScavengeableObject;
class UScavengerComponent;
enum class EWeaponName : uint8;
class AWeaponPickup;
class USecondaryWeapon;
class AInfantryWeaponMaster;
class RTS_SURVIVAL_API UWeaponState;
class USquadUnitAnimInstance;
class AAISquadUnit;
class ASquadController;
enum class EAbilityID : uint8;


/**
 * @brief Configures and applies ragdoll behaviour for a squad unit when it dies.
 */
USTRUCT(BlueprintType)
struct FSquadUnitRagdoll
{
	GENERATED_BODY()

	FSquadUnitRagdoll();

	/**
	 * @brief Starts ragdoll simulation and destroys the active weapon for the owning unit.
	 * @param OwningActor Actor owning this ragdoll configuration, used for error reporting and timers.
	 * @param MeshComponent Skeletal mesh that ragdoll physics will be applied to.
	 * @param AnimInstance Current animation instance driving the mesh; may be nullptr.
	 * @param InfantryWeapon Infantry weapon that should be destroyed when ragdoll starts; may be nullptr.
	 */
	void StartRagdoll(
		AActor* OwningActor,
		USkeletalMeshComponent* MeshComponent,
		USquadUnitAnimInstance* AnimInstance,
		AInfantryWeaponMaster* InfantryWeapon);


	/** Whether this unit should use ragdoll on death. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll")
	bool bEnableRagdoll;

	/** Collision profile name to use while ragdolled (defaults to "Ragdoll" when empty). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll")
	FName RagdollCollisionProfileName;

	/** Whether to apply an impulse when the ragdoll starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll")
	bool bApplyImpulseOnDeath;
	

	/** Strength of the impulse applied in the specified direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll",
		meta = (EditCondition = "bApplyImpulseOnDeath"))
	float ImpulseStrength;

	/** Direction of the impulse (local space) applied when the unit dies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll",
		meta = (EditCondition = "bApplyImpulseOnDeath"))
	FVector ImpulseDirection;

	/** Bone from which the impulse should be applied (for example, "pelvis"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll",
		meta = (EditCondition = "bApplyImpulseOnDeath"))
	FName ImpulseBoneName;

	/** Time in seconds before ragdoll physics is turned off (<= 0 disables this behaviour). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll")
	float PhysicsTime;

	/** Time in seconds after death before the unit actor is destroyed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll")
	float TimeTillDeath;

	/** Linear damping applied to all ragdoll bodies (acts like friction in translation). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Physics")
	float LinearDamping;

	/** Angular damping applied to all ragdoll bodies (acts like friction in rotation). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Physics")
	float AngularDamping;

	/** Mass scale applied to all ragdoll bodies (higher = heavier / less movement). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Physics")
	float MassScale;

	/** Whether to clamp initial ragdoll velocities to avoid the body flying too far. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Physics")
	bool bClampInitialVelocities;

	/** Maximum allowed initial speed for ragdoll bodies when clamping is enabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Physics",
		meta = (EditCondition = "bClampInitialVelocities"))
	float MaxInitialSpeed;

	/**
	 * @brief Optional full-body death montages played after physics is disabled.
	 * If non-empty and there is time left between PhysicsTime and TimeTillDeath,
	 * the first valid montage in this list is played.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Montage")
	TArray<TObjectPtr<UAnimMontage>> DeathMontages;

	/** True after ragdoll has been activated for this unit. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ragdoll")
	bool bIsRagdollActive;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll|Effects")
	FCollapseFX DeathEffects;


private:
	void StopAnimations(USquadUnitAnimInstance* AnimInstance, USkeletalMeshComponent* MeshComponent) const;
	void SetupPhysicsAndCollision(USkeletalMeshComponent* MeshComponent) const;
	void ApplyImpulse(USkeletalMeshComponent* MeshComponent) const;
	void ClampInitialVelocities(USkeletalMeshComponent* MeshComponent) const;
	void ScheduleDisablePhysics(
		AActor* OwningActor,
		USkeletalMeshComponent* MeshComponent,
		USquadUnitAnimInstance* AnimInstance) const;
	void DestroyWeapon(AInfantryWeaponMaster* InfantryWeapon) const;
	void StartRagdoll_CreateEffect(AActor* OwningActor);
};

USTRUCT(BlueprintType)
struct FSquadUnitHealthResistanceOverwrite
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category ="Resistance")
	EResistancePresetType ResistancePresetType = EResistancePresetType::None;
	// Gets multiplied with the health provided by the squad.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category ="Resistance")
	float HealthMlt = 1.0;
};

/**
 * @brief Represents a patrol state for a squad unit.
 */
USTRUCT()
struct FSquadUnitPatrol
{
	GENERATED_BODY()

	/** Whether the patrol end is active. */
	bool bIsToEndPatrolActive;

	/** The starting location of the patrol. */
	FVector PatrolStartLocation;

	/** The ending location of the patrol. */
	FVector PatrolEndLocation;
};

/**
 * @brief Represents a squad unit in the game.
 */
UCLASS()
class RTS_SURVIVAL_API ASquadUnit : public ACharacterObjectsMaster, public IExperienceProvider,
                                    public IRTSNavAgentInterface, public IAimOffsetProvider
{
	GENERATED_BODY()

	friend class ASquadController;
	// To call the movement functions.
	friend class URepairComponent;
	friend class UScavengerComponent;
	// to call on Squad unit spawned.
	friend class USquadReinforcementComponent;
	// for weapon swapping.
	friend struct FSquadWeaponSwitch;

public:
	ASquadUnit(const FObjectInitializer& ObjectInitializer);

	bool GetIsUnitIdle() const;
	bool GetIsUnitInCombat( )const;

	// Ovewrites if an enum was set to do so.
	void OnSquadInitsData_OverwriteArmorAndResistance(const float MyMaxHealth) const;

	// Called when we enter or exit cargo.
	void OnUnitEnteredLeftCargo(UCargo* CargoComponentEntered, const bool bEnteredCargo) const;
	void PlaySpatialVoiceLine(const ERTSVoiceLine VoiceLineType, const bool bIgnorePlayerCooldown) const;


	int32 GetOwningPlayer() const;
	// Used on the evasion components; is this unit not already evading and not active in any ability of its squad controller?
	// Then the evasion component can order it to move out of the way.
	bool GetIsSquadUnitIdleAndNotEvading() const;

	// Does not trigger any logic on the squad controller, simply strafes to the location.
	void MoveToEvasionLocation(const FVector& EvasionLocation);

	// Called by the CargoSquad component once the unit has entered cargo to ensure it will not move 
	void Force_TerminateMovement();
	/** 
	 * @return The squad controller of this unit, if valid; otherwise, nullptr.
	 */
	TObjectPtr<ASquadController> GetSquadControllerChecked() const;

	/** 
	 * @return The AI controller class of this unit, if valid; otherwise, attempts to repair it.
	 * Returns nullptr if repairs fail.
	 */
	TObjectPtr<AAISquadUnit> GetAISquadUnitChecked();

	/** 
	 * Sets the squad controller for this unit.
	 * @param SquadController The new squad controller.
	 */
	void SetSquadController(ASquadController* SquadController);

	/** 
	 * @return The weapon state of this unit's infantry weapon, if valid; otherwise, nullptr.
	 */
	UWeaponState* GetWeaponState() const;

	/** 
	 * @brief Returns whether the unit has a secondary weapon stored already
	 * @param OutIsSecondaryWpCompValid Whether the secondary weapon component is valid.
	 */
	bool GetHasSecondaryWeapon(bool& OutIsSecondaryWpCompValid) const;

	/** @return The socket to attach the secondary weapon mesh to. */
	FName GetSecondaryWeaponSocketName() const { return SecondaryWeaponSocketName; }

	void OnSpecificTargetInRange();
	void OnSpecificTargetOutOfRange(const FVector& TargetLocation);
	void OnSpecificTargetDestroyedOrInvisible();

	// IExperienceProvider interface
	virtual int32 GetExperienceWorth() const override final { return M_ExperienceWorth; };

	virtual ERTSNavAgents GetRTSNavAgentType() const override;


	virtual void GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const override;

	void OnWeaponKilledTarget() const;
	void OnWeaponFire();
	void OnProjectileHit(const bool bBounced);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;


	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category ="Resistance")
	FSquadUnitHealthResistanceOverwrite OverwriteUnitArmorAndResistance;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PhysicalMaterials")
	UPhysicalMaterial* PhysicalMaterialOverride;
	
	/** Settings and logic for ragdoll behaviour when this unit dies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ragdoll")
	FSquadUnitRagdoll M_RagdollSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AimOffset")
	TArray<FVector> AimOffsetPoints = {
		FVector(0, 0, 33), FVector(0, 0, 60),
		FVector(0, 0, 80)
	};

	// Defines which version of ANavData (Different instances of RecastNavMesh) this pawn can use.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	ERTSNavAgents NavAgentType = ERTSNavAgents::Character;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference")
	TObjectPtr<UFowComp> FowComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	USquadUnitSpatialVoiceLinePlayer* SpatialVoiceLinePlayer;

	bool GetIsValidSpatialVoiceLinePlayer() const;

	// Used to affect navmesh when stationary.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Components")
	URTSNavCollision* RTSNavCollision;


	void OnSquadSpawned(const bool bSetDisabled, const float TimeNotSelectable, const FVector& SquadUnitLocation);
	void StrafeToLocation(const FVector& StrafeLocation);

	// setup RTS comp, AI controller and Squad Anim instance.
	virtual void PostInitializeComponents() override;
	/** @brief Initializes the Scavenger component with owner. */
	void PostInitializeComp_InitScavengerComponent();
	void PostInitializeComp_SetupSpatialVoiceLinePlayer();
	/** @brief Initialzies the Repair component with owner. */
	void PostInitializeComp_InitRepairComponent();
	/** @brief Initializes the Secondary weapon with owner. */
	void PostInitializeComp_InitSecondaryWeapon();
	/** @brief Sets the collision for the infantry capsule component. */
	void PostInitializeComp_SetupInfantryCapsuleCollision() const;
	/** @brief Sets the M_AISquadUnit reference, may spawn a controller if none is found. */
	void PostInitializeComp_SetupAISquadUnit();
	/** @brief Initializes the ANimBP_SquadUnit reference. If a valid one is found we set the reference to our mesh on it. */
	void PostInitializeComp_SetupAnimBP();


	/** Moves the unit along the provided path. */
	void ExecuteMoveAlongPath(const FNavPathSharedPtr& Path, const EAbilityID AbilityToMoveFor);

	/**
	 * @brief Moves the unit to the provided location using pathfinding on its own controller.
	 * @param MoveToLocation The target location to move to.
	 * @param AbilityToMoveFor
	 * @post The active ability ID is set to IdMoveTo.
	 */
	void ExecuteMoveToSelfPathFinding(const FVector& MoveToLocation, const EAbilityID AbilityToMoveFor,
	                                  bool bUsePathfinding = true);

	/**
	 * @brief Stops the current movement command and sets the active ability ID to Idle.
	 */
	void TerminateMovementCommand();

	/**
	 * @brief Moves the unit to the provided location and binds OnMoveCompleted to the MoveToLocation.
	 * @note This function does not alter the active ability ID.
	 * @param MoveToLocation The target location to move to.
	 * @param bUsePathfinding Whether to path find to the location or move directly.
	 * @param MoveContext
	 */
	void MoveToAndBindOnCompleted(const FVector& MoveToLocation, bool bUsePathfinding = true,
	                              const EAbilityID MoveContext = {});

	void MoveToActorAndBindOnCompleted(
		AActor* TargetActor,
		float AcceptanceRadius,
		EAbilityID AbilityToMoveFor);


	/**
	 * @brief Commands the unit to attack a specified target.
	 * @param TargetActor The target actor to attack.
	 */
	void ExecuteAttackCommand(AActor* TargetActor);

	void ExecuteAttackGroundCommand(const FVector& TargetLocation);
	void TerminateAttackGroundCommand();

	/**
	 * @brief Terminates the current attack command and sets the active ability ID to Idle.
	 */
	void TerminateAttackCommand();

	/**
	 * @brief Initiates a patrol command for the unit between two locations.
	 * @param StartLocation The starting location of the patrol.
	 * @param PatrolToLocation The ending location of the patrol.
	 */
	void ExecutePatrol(const FVector& StartLocation, const FVector& PatrolToLocation);

	/**
	 * @brief Terminates the current patrol command and sets the active ability ID to Idle.
	 */
	void TerminatePatrol();

	void ExecuteSwitchWeapon();
	void TerminateSwitchWeapon();

	/**
	 * @brief Rotates the unit towards a specified rotation.
	 * @param RotateToRotator The target rotation.
	 */
	void ExecuteRotateTowards(const FRotator& RotateToRotator);

	/** The animation instance for this squad unit. */
	UPROPERTY(BlueprintReadOnly)
	USquadUnitAnimInstance* AnimBp_SquadUnit;

	/**
	 * @brief Sets up the infantry weapon for this squad unit.
	 * @param NewWeapon The new weapon to assign to the unit.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupWeapon(AInfantryWeaponMaster* NewWeapon);

        void TerminatePickupWeapon();

        /**
         * @brief Exchanges the primary weapon child actor with another squad unit.
         * @param OtherUnit The unit to swap weapons with.
         * @return True when both units had valid weapons and the swap completed.
         */
        bool SwapWeaponsWithUnit(ASquadUnit* OtherUnit);

	/**
	 * @brief Sets the infantry weapon to automatically engage targets in range.
	 * @param bUseLastTarget Whether to use the last targeted actor in the first auto-engage loop.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetWeaponToAutoEngageTargets(const bool bUseLastTarget);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName SecondaryWeaponSocketName;

	/**
	 * @brief This unit was chosen by the sq controller to pick up the provided weapon,
	 *  will now switch weapon and disable the currently held weapon.
	 * @param TargetWeaponItem The new weapon to pick up.
	 * @post The item is destroyed and the unit will have started the switch weapon animation.
	 */
	void StartPickupWeapon(AWeaponPickup* TargetWeaponItem);

	/** Checks if the unit has a scavenger component */
	bool GetHasValidScavengerComp() const;
	bool GetHasValidRepairComp() const;

	/** Gets the scavenger component */
	UScavengerComponent* GetScavengerComponent() const { return M_ScavengerComponent; }
	URepairComponent* GetRepairComponent() const { return M_RepairComponent; }

	/** Terminates scavenging */
	void TerminateScavenging();

	/**
	 * @brief Handles the death of the unit and cleans up references and components.
	 */
	virtual void UnitDies(const ERTSDeathType DeathType) override;

	// Component that contains the secondary weapon for this unit.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference")
	TObjectPtr<USecondaryWeapon> M_SecondaryWeapon;

	void OnScavengeStart(UStaticMesh* ScavengeEquipment, FName ScavengeSocketName, float TotalScavengeTime,
	                     const TObjectPtr<AScavengeableObject>
	                     & ScaveObj, const TObjectPtr<UNiagaraSystem>& Effect, FName EffectSocket);

private:
	/** The current active command of the unit. */
	EAbilityID M_ActiveCommand;

	/** The squad controller managing this unit. */
	UPROPERTY()
	TObjectPtr<ASquadController> M_SquadController;

	/** Timer handle for updating the animation. */
	FTimerHandle M_TimerHandleUpdateAnim;

	FTimerHandle M_TimerHandleWeaponSwitch;

	FTimerHandle M_DeathTimerHandle;

	UPROPERTY()
	int32 OwningPlayer;

	void SetOwningPlayerAndStartFow(const int32 NewOwningPlayer, const float NewVisionRange);


	/** The AI controller for this squad unit. */
	UPROPERTY()
	TObjectPtr<AAISquadUnit> M_AISquadUnit;

	/** The path the unit is following. */
	FNavPathSharedPtr M_CurrentPath;

	/** 
	 * @return True if the squad controller is valid; otherwise, false.
	 */
	bool GetIsValidSquadController() const;

	/** 
	 * @return True if the AI controller is valid; otherwise, false.
	 */
	bool GetIsValidAISquadUnit();

	bool GetIsValidSecondaryWeapon();

	/**
	 * @return Whether the child actor component is valid, if not attempts to retrieve it.
	 */
	bool GetIsValidChildWeaponActor();

	/** 
	 * @return True if the infantry weapon is valid; otherwise, false.
	 */
	bool GetIsValidWeapon() const;

	/** 
	 * @brief Notifies the squad controller of the completion of the current command.
	 */
	void OnCommandComplete();

	/**
	 * @brief Callback function for when the unit's movement to a location is complete.
	 * @param RequestID The ID of the movement request that was completed.
	 * @param Result The result of the path following request.
	 * @note If the active command is a patrol command, the unit will swap between patrol points.
	 * Otherwise, the unit will call OnCommandComplete.
	 */
	UFUNCTION()
	void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

	void OnMoveCompleted_Patrol();
	void OnMoveCompleted_Scavenge();
	void OnMoveCompleted_Repair() const;
	void OnMoveCompleted_Capture() const;
	void OnMoveCompleted_MoveCloserToTarget();

        void DetermineDeathVoiceLine();

        void SetupSwappedWeapon(AInfantryWeaponMaster* NewWeapon);

	/** The infantry weapon assigned to this squad unit. */
	UPROPERTY()
	TObjectPtr<AInfantryWeaponMaster> M_InfantryWeapon;

	/** The current patrol state of the unit. */
	FSquadUnitPatrol M_PatrolState;

	/** The target actor for the unit to engage. */
	UPROPERTY()
	AActor* M_TargetActor;

	// Reference to the child actor comp that holds the weapon that is active.
	UPROPERTY()
	UChildActorComponent* M_ChildWeaponComp;

	UPROPERTY()
	URTSSquadUnitOptimizer* M_OptimizationComp;

	// Callback to when secondary weapon from pickup is loaded.
	void OnSecondaryWeaponLoaded(const EWeaponName NewWeaponName, TSoftClassPtr<AInfantryWeaponMaster> NewWeaponClass);

	// Precondition check: Ensure that required components are valid.
	bool OnSecondaryWeapon_ValidatePreconditions();

	// If the current primary weapon is valid, record its class and weapon name,
	// disable its search, and setup the secondary weapon mesh accordingly.
	void OnSecondaryWeapon_TransferPrimaryWeaponDetailsToSecondary(
		TSoftClassPtr<AInfantryWeaponMaster>& OutOldPrimaryWeaponClass,
		EWeaponName& OutOldPrimaryWeaponName);

	// Destroys the current child actor (if any) and resets the child actor class.
	void OnSecondaryWeapon_DestroyOldWeaponActor() const;

	// Sets the new weapon class on the child actor component, spawns the new actor,
	// and if valid, initializes it by setting the owner, owning player, updating the animation,
	// and scheduling the switch weapon completion.
	void OnSecondaryWeapon_SpawnAndInitializeNewWeapon(TSoftClassPtr<AInfantryWeaponMaster> NewWeaponClass);

	/** @brief Sets the M_ScasvengeEquipmentMesh component with provided mesh and attaches it to the parent mesh
	 * at the provided socket name. If that is successful we also create the Effect on that component. */
	void OnScavengeStart_SetupScavengeEquipmentMesh(UStaticMesh* ScavengeEquipment, const FName& ScavengeSocketName,
	                                                const TObjectPtr<UNiagaraSystem>& Effect,
	                                                const FName& EffectSocketName);


	// Checks what the current active ability is, if this is an attack ability then the unit will engage the target.
	// Else sets the weapon to auto engage targets.
	void OnSwitchWeaponCompleted();

	/**
	 * @brief Checks if there is not a weapon switch already active, if there is
	 * we return false.
	 * @return Whether the weapon switch is allowed.
	 */
	bool GetIsWeaponSwitchAllowedToStart();

	// If the unit is already switching weapons then do not allow a new switch command as it uses
	// asynchronous loading.
	bool bM_IsSwitchingWeapon;

	/** The scavenger component of this unit,
	 * is set at post init components automatically if implemented on the derived blueprint.*/
	UPROPERTY()
	TObjectPtr<UScavengerComponent> M_ScavengerComponent;

	// The repair component of this unit, is set at post init components automatically
	// if implemented on the derived blueprint.
	UPROPERTY()
	TObjectPtr<URepairComponent> M_RepairComponent;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ScavengeEquipmentMesh;

	void AttachEffectAtEquipmentMesh(UNiagaraSystem* Effect, FName EffectSocket);

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_ScavengeEquipmentEffect;

	FTimerHandle M_SpawnSelectionTimerHandle;

	void ReportPathFollowingResultError(const EPathFollowingResult::Type Result) const;

	/** @brief Uses ErrorReporting if no child weapon actor is found. */
	void BeginPlay_SetupChildActorWeaponComp();
	/** @brief Sets up a lambda function to call periodically to update the speed of the unit on the animation instance. */
	void BeginPlay_SetupUpdateAnimSpeed();
	// Disable affection of nav mesh on selection area, decal and healthbar.
	void BeginPlay_SetupSelectionHealthCompCollision() const;

	void BeginPlay_SetPhysicalMaterials() const;

	/** @brief Handles selection updates and command completion when the unit dies. */
	void UnitDies_HandleSelectionAndCommand(bool& OutIsSelected);

	/** @brief Removes the unit from the squad controller. */
	void UnitDies_RemoveFromSquadController(bool bIsSelected);

	/** @brief Notifies the animation instance that the unit is dying. */
	void UnitDies_NotifyAnimInstance();

	/** @brief Destroys the infantry weapon if it is valid. */
	void UnitDies_DestroyInfantryWeapon();

	/** @brief Removes the unit from the AI controller. */
	void UnitDies_RemoveFromAIController();

	/** @brief Clears timers, hides the health bar, disables collision, and deregisters from the game state. */
	void UnitDies_CleanupComponents();

	/** @brief Schedules the destruction of the unit after a delay. */
	void UnitDies_ScheduleDestruction();

	void StopMovementAndClearPath();

	void Debug_Weapons(const FString& DebugMessage, const FColor Color);

	void OnDataLoaded_InitExperienceWorth();
	// Set after the squad controller is set so value can be aligned with squad data value.
	float M_ExperienceWorth;

	void OnMoveToSelfPathFindingFailed(
		const EPathFollowingRequestResult::Type RequestResult,
		const FVector& MoveToLocation, const EAbilityID MoveContext);

	void OnMoveToActorRequestFailed(const AActor* TargetActor,
	                                const EAbilityID AbilityToMoveFor,
	                                const bool bFailedBeforeMakingRequest,
	                                EPathFollowingRequestResult::Type RequestResult);

	FAIMoveRequest CreateMoveToActorRequest(
		AActor* GoalActor, const float AcceptanceRadius,
		const bool bAllowPartialPathFinding = true);


	// Performs RTS error check.
	bool EnsureRepairComponentIsValid() const;

	void OnSquadUnitSpawned_SetEnabled(const float TimeNotSelectable, const FVector& SquadUnitLocation);
	void OnSquadUnitSpawned_TeleportAndSetSelectable(const FVector& SquadUnitLocation);

	UPROPERTY()
	float M_LastPatrolMoveTimeOut = 0.f;
};
