// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpCharacterObjectsMaster.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderTypes.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnitPlannedPosition.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceInterface/ExperienceInterface.h"
#include "RTS_Survival/RTSComponents/NavCollision/RTSNavCollision.h"
#include "RTS_Survival/Weapons/AimOffsetProvider/AimOffsetProvider.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "SquadUnit.generated.h"

struct FRTSCombatCoverThreats;

class USquadUnitSpatialVoiceLinePlayer;
class UCargo;
class URTSCoverFinderWorldSubsystem;
enum class ERTSNavAgents : uint8;
class URTSSquadUnitOptimizer;
class URepairComponent;
class UFowComp;
class AScavengeableObject;
class UScavengerComponent;
enum class EWeaponName : uint8;
enum class ESquadIdleAnimationPose : uint8;
enum class ESquadCoverAnimAction : uint8;
struct FSquadUnitCoverLocalOffset;
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
	 * @brief Starts ragdoll simulation after the death montage has completed.
	 * @param OwningActor Actor owning this ragdoll configuration, used for error reporting and timers.
	 * @param MeshComponent Skeletal mesh that ragdoll physics will be applied to.
	 * @param AnimInstance Current animation instance driving the mesh; may be nullptr.
	 */
	void StartRagdoll(
		AActor* OwningActor,
		USkeletalMeshComponent* MeshComponent,
		USquadUnitAnimInstance* AnimInstance);


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

	/** Time in seconds that ragdoll physics runs after the death montage before the unit is destroyed. */
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

/** The unit-local lifecycle of an assigned cover point. */
UENUM(BlueprintType)
enum class ESquadUnitCoverState : uint8
{
	None UMETA(DisplayName = "None"),
	Assigned UMETA(DisplayName = "Assigned"),
	MovingToCover UMETA(DisplayName = "Moving To Cover"),
	EnteringCover UMETA(DisplayName = "Entering Cover"),
	Protected UMETA(DisplayName = "Protected"),
	Exposed UMETA(DisplayName = "Exposed"),
	LeavingCover UMETA(DisplayName = "Leaving Cover")
};

/** The tactical reason for using cover, kept separate from the unit's active command. */
UENUM(BlueprintType)
enum class ESquadUnitCoverUseReason : uint8
{
	None UMETA(DisplayName = "None"),
	Attack UMETA(DisplayName = "Attack"),
	AfterMoveCommand UMETA(DisplayName = "After Move Command")
};

/**
 * @brief Stores the cover assignment and lifecycle without owning movement or command execution.
 * The copied point remains a snapshot that the future cover integration must revalidate before use.
 */
USTRUCT(BlueprintType)
struct FSquadUnitCoverRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cover")
	ESquadUnitCoverState State = ESquadUnitCoverState::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cover")
	ESquadUnitCoverUseReason UseReason = ESquadUnitCoverUseReason::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cover")
	FRTSCoverPoint AssignedCoverPoint;

	bool GetHasAssignment() const;
	bool GetIsOccupyingCover() const;
	void Reset();
};

/**
 * @brief Notices a soldier that keeps circling its player-planned destination instead of arriving.
 * Planned destinations sit much closer together than the squad's default spread, and a running unit that is
 * nudged off line next to its goal cannot turn tightly enough to reach it.
 */
USTRUCT()
struct FSquadUnitPlannedMoveWatch
{
	GENERATED_BODY()

	// World time at which the unit first got close to its destination while still walking; negative when not close.
	float NearGoalSinceWorldSeconds = -1.0f;

	// The unit is stopped and sent again once; if it still does not arrive it finishes its move where it stands.
	bool bHasRestartedFromStandstill = false;

	// A unit that could not start walking keeps trying until this world time, then finishes where it stands.
	float StartRetryDeadlineWorldSeconds = -1.0f;
};

/**
 * @brief Moves the capsule onto a known cover location over a few frames instead of popping it there.
 * Lines the unit up before an enter clip and absorbs whatever a root-motion clip left over at its end.
 */
USTRUCT()
struct FSquadUnitCoverCapsuleSlide
{
	GENERATED_BODY()

	FVector StartLocation = FVector::ZeroVector;

	FVector TargetLocation = FVector::ZeroVector;

	float DurationSeconds = 0.0f;

	float ElapsedSeconds = 0.0f;

	FTimerHandle TimerHandle;

	bool bIsActive = false;
};

/**
 * @brief Stops the walk to cover from stalling with the weapon lowered.
 * Deadlines are world times; the flags track which recovery steps the current assignment already used.
 */
USTRUCT()
struct FSquadUnitCoverMoveGuard
{
	GENERATED_BODY()

	// No new cover search starts before this time, so failed or just-cancelled searches cannot repeat every update.
	float NextSearchWorldSeconds = 0.0f;

	// Past this time the current step (walk to cover or pose hand-over) is retried or abandoned.
	float StepDeadlineWorldSeconds = 0.0f;

	// A unit circling its point is stopped and sent again once before the point is reported unreachable.
	bool bHasRestartedApproach = false;

	// The point this unit last left because it could not engage its target from there, and until when it is
	// skipped. Without this a unit whose target flickers walks back into the same point again and again.
	int64 RejectedPointId = 0;
	float RejectedPointExpiryWorldSeconds = 0.0f;

	// When the current point was assigned; a unit holds a point for a while before it may trade it for a better one.
	float AssignedWorldSeconds = 0.0f;

	// A unit in cover compares its point with the ones around it no earlier than this.
	float NextCombatEvaluationWorldSeconds = 0.0f;
};

/**
 * @brief Represents a squad unit in the game.
 *
 * Cover is tracked as a subordinate tactical state in M_CoverRuntimeState and never replaces M_ActiveCommand.
 * SetCoverAssignment records the selected point and whether it supports an attack or follows a move command.
 * SetCoverState then advances the unit through movement, entry, protection, exposure, and exit without reporting
 * command completion. ClearCoverState removes the assignment. URTSCoverFinderWorldSubsystem calls
 * UpdateAutomaticCover on a staggered schedule; that update finds, reserves, walks to, and validates cover while the
 * unit is idle or attacking a target already in range, and any commanded movement cancels it.
 */
UCLASS()
class RTS_SURVIVAL_API ASquadUnit : public ACharacterObjectsMaster, public IExperienceProvider,
                                    public IRTSNavAgentInterface, public IAimOffsetProvider
{
	GENERATED_BODY()

	friend class ASquadController;
	friend class ATeamWeaponController;
	// To call the movement functions.
	friend class URepairComponent;
	friend class UScavengerComponent;
	// to call on Squad unit spawned.
	friend class USquadReinforcementComponent;
	// for weapon swapping.
	friend struct FSquadWeaponSwitch;
	// for changing the way the weapon of this unit behaves when an aim ability is active.
	friend class USquadAimAbilityComponent;
	friend class URTSCoverFinderWorldSubsystem;

public:
	ASquadUnit(const FObjectInitializer& ObjectInitializer);

	bool GetIsUnitIdle() const;
	bool GetIsUnitInCombat( )const;

	const FSquadUnitCoverRuntimeState& GetCoverRuntimeState() const { return M_CoverRuntimeState; }
	bool GetHasCoverAssignment() const { return M_CoverRuntimeState.GetHasAssignment(); }
	bool GetIsOccupyingCover() const { return M_CoverRuntimeState.GetIsOccupyingCover(); }
	EAbilityID GetActiveCommand() const { return M_ActiveCommand; }

	/**
	 * @brief Lets cover selection test the firing lane from where this unit's expose animation really ends.
	 * @param CoverPoint Standing point being evaluated; crouch points have no exposed step.
	 * @param DefaultWorldOffset Step to use when the unit has no expose montage for that side.
	 * @return World-space offset from the cover point to the exposed capsule position.
	 */
	FVector GetStandingCoverExposedWorldOffset(
		const FRTSCoverPoint& CoverPoint,
		const FVector& DefaultWorldOffset) const;

	/**
	 * @brief Measures how far a resting unit is from where the cover logic believes it stands.
	 * @param OutErrorCentimeters Horizontal distance between the capsule and its expected cover location.
	 * @return False while the unit is moving, sliding, or playing a cover transition.
	 */
	bool TryGetSettledCoverCapsuleError(float& OutErrorCentimeters) const;

	// Lets cover selection skip the point this unit just gave up because its target could not be engaged from it.
	bool GetIsCoverPointRejectedForTarget(int64 PointId) const;

	/**
	 * @brief Stores where the player's move preview put this soldier, after its squad issued the move.
	 * A planned cover point is taken on arrival; a planned open position is held instead of wandering to cover.
	 * @param PlannedPosition Position from the squads-only move preview.
	 */
	void SetPlayerPlannedPosition(const FSquadUnitPlannedPosition& PlannedPosition);
	const FSquadUnitPlannedPosition& GetPlayerPlannedPosition() const { return M_PlayerPlannedPosition; }

	/**
	 * @return Where this soldier should stop walking for a planned position: the spot its enter animation
	 * starts from for cover, the position itself otherwise.
	 */
	FVector GetArrivalLocationForPlannedPosition(const FSquadUnitPlannedPosition& PlannedPosition) const;

	/**
	 * @brief Records a tactical cover assignment without replacing or completing the active command.
	 * @param CoverPoint Immutable snapshot supplied by the future cover-selection integration.
	 * @param UseReason Why the unit should use this point while its main command continues independently.
	 * @return True when the assignment was accepted.
	 */
	bool SetCoverAssignment(const FRTSCoverPoint& CoverPoint, ESquadUnitCoverUseReason UseReason);

	/**
	 * @brief Advances only the unit-local cover lifecycle and never changes the active command.
	 * @param NewState New lifecycle state; None clears the complete cover assignment.
	 * @return True when the state change was accepted.
	 */
	bool SetCoverState(ESquadUnitCoverState NewState);

	void ClearCoverState();
	
	/** Gets the scavenger component */
	URepairComponent* GetRepairComponent() const { return M_RepairComponent; }

	inline AInfantryWeaponMaster* GetInfantryWeapon() const { return M_InfantryWeapon; }
	inline  USquadUnitAnimInstance * GetAnimBP_SquadUnit() const { return AnimBp_SquadUnit; }

	// Ovewrites if an enum was set to do so.
	void OnSquadInitsData_OverwriteArmorAndResistance(const float MyMaxHealth) const;

	// Called when we enter or exit cargo.
	void OnUnitEnteredLeftCargo(UCargo* CargoComponentEntered, const bool bEnteredCargo) const;
	void PlaySpatialVoiceLine(const ERTSVoiceLine VoiceLineType, const bool bIgnorePlayerCooldown) const;
	
	/**
	 * @brief Moves the unit to the provided location using pathfinding on its own controller.
	 * @param MoveToLocation The target location to move to.
	 * @param AbilityToMoveFor
	 * @post The active ability ID is set to IdMoveTo.
	 */
	void ExecuteMoveToSelfPathFinding(const FVector& MoveToLocation, const EAbilityID AbilityToMoveFor,
	                                  bool bUsePathfinding = true);


	int32 GetOwningPlayer() const;
	// Used on the evasion components; is this unit not already evading and not active in any ability of its squad controller?
	// Then the evasion component can order it to move out of the way.
	bool GetIsSquadUnitIdleAndNotEvading() const;

	// True while the AI controller is still moving this unit along a path, whatever ability issued the move.
	bool GetIsPathFollowingActive() const;

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
	virtual void SetSquadController(ASquadController* SquadController);

	/** 
	 * @return The weapon state of this unit's infantry weapon, if valid; otherwise, nullptr.
	 */
	UWeaponState* GetWeaponState() const;

	/** 
	 * @brief Returns whether the unit has a secondary weapon stored already
	 * @param OutIsSecondaryWpCompValid Whether the secondary weapon component is valid.
	 */
	bool GetHasSecondaryWeapon(bool& OutIsSecondaryWpCompValid);

	/** @return The socket to attach the secondary weapon mesh to. */
	FName GetSecondaryWeaponSocketName() const { return SecondaryWeaponSocketName; }

	virtual void OnSpecificTargetInRange();
	virtual void OnSpecificTargetOutOfRange(const FVector& TargetLocation);
	virtual void OnSpecificTargetDestroyedOrInvisible();

	// IExperienceProvider interface
	virtual int32 GetExperienceWorth() const override final { return M_ExperienceWorth; };

	virtual ERTSNavAgents GetRTSNavAgentType() const override;


	virtual void GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const override;

	void OnWeaponKilledTarget(AActor* KilledActor) const;
	void OnWeaponFire();
	void OnProjectileHit(const bool bBounced);

	/** 
	 * @return True if the infantry weapon is valid; otherwise, false.
	 */
	bool GetIsValidWeapon() const;

	void SetNoDeathVoiceLineOnDeath();

	UScavengerComponent* GetScavengerComponent() const { return M_ScavengerComponent; }
	

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
	virtual void StrafeToLocation(const FVector& StrafeLocation);

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
	 * @brief Stops the current movement command and sets the active ability ID to Idle.
	 */
	virtual void TerminateMovementCommand();

	virtual void TerminateMovementCommandDoNotKillVelocity();

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
	TObjectPtr<USquadUnitAnimInstance> AnimBp_SquadUnit;

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

	// Cover remains subordinate to the active command so tactical positioning cannot complete or replace it.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cover", meta = (AllowPrivateAccess = "true"))
	FSquadUnitCoverRuntimeState M_CoverRuntimeState;

	// Preserves whether range-closing movement belongs to Attack or AttackGround.
	EAbilityID M_CombatAbilityBeforeRangeClosing = EAbilityID::IdNoAbility;
	FAIRequestID M_RangeClosingRequestID = FAIRequestID::InvalidRequest;
	FAIRequestID M_CoverMoveRequestID = FAIRequestID::InvalidRequest;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_CoverIgnoredProviderActor;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_CoverValidatedTarget;

	FVector M_CoverValidatedTargetLocation = FVector::ZeroVector;

	UPROPERTY()
	FSquadUnitCoverCapsuleSlide M_CoverCapsuleSlide;

	// Set by the squad when a player-planned move starts; replaced or cleared by the next commanded movement.
	FSquadUnitPlannedPosition M_PlayerPlannedPosition;

	UPROPERTY()
	FSquadUnitPlannedMoveWatch M_PlannedMoveWatch;

	FTimerHandle M_PlannedMoveStartRetryTimer;

	// The request the unit currently walks its planned move with. Re-sending a planned move aborts the previous
	// request, and that abort arrives after the new one started; it must not end the squad's command.
	FAIRequestID M_PlannedMoveRequestID;

	UPROPERTY()
	FSquadUnitCoverMoveGuard M_CoverMoveGuard;

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

	/** @return True when the mesh-owned squad animation instance is available. */
	bool GetIsValidAnimBpSquadUnit() const;

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
	void OnMoveCompleted_Cover(EPathFollowingResult::Type Result);

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
	void BeginPlay_BindSelectionFunctions();

	/** @brief Handles selection updates and command completion when the unit dies. */
	void UnitDies_HandleSelectionAndCommand(bool& OutIsSelected);

	/** @brief Removes the unit from the squad controller. */
	void UnitDies_RemoveFromSquadController(bool bIsSelected, ERTSDeathType DeathType);

	/** @brief Notifies the animation instance that the unit is dying. */
	void UnitDies_NotifyAnimInstance();

	/** @return Whether the animation Blueprint should select a crouched death montage for this death. */
	bool UnitDies_GetShouldUseCrouchedDeathMontage() const;

	/** Starts the selected montage and schedules the death transition shortly before its natural end. */
	void UnitDies_StartDeathSequence(bool bUseCrouchedDeathMontage);

	/** Continues into ragdoll or immediate destruction exactly once after the montage phase. */
	void UnitDies_OnDeathMontageFinished();

	/** @param MontageDuration Actual one-pass duration used to transition at 95 percent playback. */
	void UnitDies_ScheduleDeathMontageTransition(float MontageDuration);

	/** @brief Destroys the infantry weapon if it is valid. */
	void UnitDies_DestroyInfantryWeapon();

	/** @brief Removes the unit from the AI controller. */
	void UnitDies_RemoveFromAIController();

	/** @brief Clears timers, hides the health bar, disables collision, and deregisters from the game state. */
	void UnitDies_CleanupComponents();

	/** @brief Schedules the destruction of the unit after a delay. */
	void UnitDies_ScheduleDestruction();

	bool bM_HasCompletedDeathMontage = false;

	void StopMovementAndClearPath();
	void UpdateAutomaticCover(URTSCoverFinderWorldSubsystem& CoverSubsystem);
	bool GetCanUseAutomaticCover() const;

	/** @return True while a unit that already holds a point may stay, which is more lenient than taking one. */
	bool GetMayKeepAutomaticCover() const;
	void LeaveCoverUnusableAgainstTarget(const TCHAR* Reason);
	bool GetIsSquadEligibleForAutomaticCover() const;
	bool GetIsCoverSearchOnCooldown() const;
	bool GetHasCoverStepTimedOut() const;
	void StartCoverStepDeadline(float DurationSeconds);
	void DelayNextCoverSearch(float DelaySeconds);
	bool GetIsCoverAnimationOutOfSync() const;
	void TryStartAutomaticCover(URTSCoverFinderWorldSubsystem& CoverSubsystem);
	bool StartCoverMovement();
	float GetCoverWalkDeadlineSeconds() const;
	void FinishCoverMovement();
	void StopCoverMovementWithoutCallback();
	void AbandonUnreachableCoverPoint();
	void EnterAssignedCover();
	void UpdateMovingToCover(URTSCoverFinderWorldSubsystem& CoverSubsystem);
	void UpdateEnteringCover();
	void UpdateProtectedCover(URTSCoverFinderWorldSubsystem& CoverSubsystem);
	void UpdateExposedCover(URTSCoverFinderWorldSubsystem& CoverSubsystem);

	/**
	 * @brief Moves a unit in cover to a clearly better point once the enemy is no longer behind its cover.
	 * Only one unit of a squad walks at a time, so the squad keeps firing while it shifts.
	 * @param CoverSubsystem Cover service that scores and reserves the points.
	 * @param TargetActor The enemy this unit is shooting at.
	 * @param TargetLocation That enemy's aim location.
	 * @return True when the unit gave up its point for a new one.
	 */
	bool TryRepositionToBetterCombatCover(
		URTSCoverFinderWorldSubsystem& CoverSubsystem,
		AActor& TargetActor,
		const FVector& TargetLocation);

	/**
	 * @brief Collects the enemies this unit wants cover from: its own target and those of its squad mates.
	 * @param TargetActor The enemy this unit is shooting at.
	 * @param TargetLocation That enemy's aim location.
	 * @return The threats, with this unit's target first.
	 */
	FRTSCombatCoverThreats BuildCombatCoverThreats(const AActor& TargetActor, const FVector& TargetLocation) const;

	// Cover must stay clearly inside weapon range, or entering it would make the squad walk closer again.
	float GetMaximumCoverDistanceToTarget() const;
	bool GetIsSquadMateWalkingToCover() const;
	void ReturnToProtectedCover();
	void ExposeFromStandingCover();
	void RequestCoverEnterAnimation();

	// Bound to the animation instance so capsule and cover state follow a transition on the frame it completes.
	void OnCoverAnimActionChanged(ESquadCoverAnimAction NewAction);
	void OnCoverAnimReachedProtected();
	void OnCoverAnimReachedExposed();

	// Polled backstop for the callback above; also ends transitions whose montage never advanced.
	void SyncCoverStateWithAnimation();
	void StartCoverTransitionDeadline();
	void LogCoverAlignmentResidual(const TCHAR* ReachedPose, const FVector& ExpectedLocation) const;

	/** @return Where the unit stops before its enter clip: the cover point plus the clip's designer offset. */
	FVector GetCoverEntryLocation() const;
	FVector GetCoverEntryLocationForPoint(const FRTSCoverPoint& CoverPoint) const;

	/**
	 * @brief Uses the player's planned position before the automatic search runs.
	 * @param CoverSubsystem Subsystem that owns the reservation of a planned cover point.
	 * @return True when the plan decided what the unit does, so no automatic search must follow.
	 */
	bool TryUsePlayerPlannedPosition(URTSCoverFinderWorldSubsystem& CoverSubsystem);
	void ApplyPlayerPlannedFacing();

	/**
	 * @brief Starts walking to a player-planned position as this unit's part of its squad's move command.
	 * @param PlannedPosition Where the squads-only move preview put this soldier.
	 */
	void ExecutePlannedMove(const FSquadUnitPlannedPosition& PlannedPosition);

	ESquadPlannedMoveStart StartPlannedMoveRequest(const FVector& ArrivalLocation);

	void ClearPlayerPlannedPosition();

	// Off screen the optimizer moves a soldier in steps longer than the arrival radius, so it steps over its slot
	// back and forth; nobody can see it being put on the slot instead.
	void PlaceUnseenUnitOnPlannedPosition(const FVector& ArrivalLocation);

	// A soldier ordered away in the middle of a root-motion cover clip cannot start walking until it ended.
	void RetryPlannedMoveStart();

	/**
	 * @brief Paths from the closest navigable spot when the unit itself stands off the navmesh.
	 * A soldier pressed against its cover can stand inside the margin the navmesh keeps around the obstacle.
	 * @param MoveRequest The request that could not be started from where the unit stands.
	 * @return True when the unit is now walking.
	 */
	bool StartPlannedMoveFromNearestNavigableLocation(const FAIMoveRequest& MoveRequest);

	// Called with the tactical cover update; finishes a planned move whose unit circles its destination.
	void UpdatePlannedMoveArrival();
	void CompletePlannedMoveInPlace();
	FVector GetCoverExposedLocation() const;
	FVector GetCoverLocalOffsetInWorld(
		const FRTSCoverPoint& CoverPoint,
		const FSquadUnitCoverLocalOffset& LocalOffset) const;

	/**
	 * @brief Puts the capsule on a cover location, sliding when it is visibly off and a duration is given.
	 * @param TargetLocation Cover point, entry location, or exposed location; height is ignored.
	 * @param SlideSeconds Zero places the capsule immediately.
	 */
	void AlignCapsuleToCoverLocation(const FVector& TargetLocation, float SlideSeconds);
	void PlaceCapsuleAtCoverLocation(const FVector& TargetLocation);
	void TickCoverCapsuleSlide();
	void FinishCoverCapsuleSlide();
	void StopCoverCapsuleSlide();
	void ClearCoverStateInternal(bool bStopCoverMovement);

	// Leaves cover and records why, so repeated enter and leave cycles can be traced in the log.
	void LeaveCoverForReason(const TCHAR* Reason);
	void CancelAutomaticCoverForCommandMovement();
	void SetCoverWeaponFireBlocked(bool bBlocked) const;
	void RegisterCoverProviderWeaponIgnore(AActor* ProviderActor, bool bRegister);
	void ApplyCurrentCoverWeaponState();
	bool GetIsCoverProviderOwnedByUnit(const AActor& ProviderActor) const;
	AActor* GetCurrentCoverTarget(FVector& OutTargetLocation) const;
	ESquadIdleAnimationPose GetAssignedCoverAnimationPose() const;
	bool GetShouldRevalidateCoverLane(const AActor& TargetActor, const FVector& TargetLocation) const;
	void RecordValidatedCoverTarget(AActor* TargetActor, const FVector& TargetLocation);
	void PrepareForRangeClosingMovement(EAbilityID MovementAbility);
	void RestoreCombatAbilityAfterRangeClosing();

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

	// Set when killed at the start of the game to simulate damage on the squad.
	bool bM_NoDeathVoiceLineOnDeath = false;
};
