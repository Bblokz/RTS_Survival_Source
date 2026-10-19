// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Interfaces/RTSInterface/RTSUnit.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInUnit/DigInUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/FRTSOverlapEvasion/RTSOverlapEvasionComponent.h"
#include "RTS_Survival/Units/Tanks/VehicleAvoidance/RTSAlliedVehiclePassage.h"
#include "RTS_Survival/Units/Tanks/VehicleAI/Utils/VehicleAIInterface.h"
#include "TrackPhysicsMovementComp/TrackPhysicsMovement.h"
#include "TrackedTankMaster.generated.h"

class UTankTargetAcquisition;
class UAttachedRockets;
class UAudioComponent;
class UBoxComponent;
class UPhysicalMaterial;
class USoundBase;
class RTS_SURVIVAL_API UChassisAnimInstance;
class RTS_SURVIVAL_API UTrackPhysicsMovement;

/**
 * @brief Stores the latest coalesced tracked-move request so nav-settle and chain continuity use one shared state source.
 * @note This struct intentionally represents only the newest request; rapid command spam overwrites stale pending data.
 */
USTRUCT()
struct FTrackedTankQueuedMoveState
{
	GENERATED_BODY()

	// Destination that will be issued once delay/coalescing policy allows movement.
	FVector M_TargetLocation = FVector::ZeroVector;
	// True while a request is waiting for deferred issue; cleared immediately after ExecuteTrackedMoveNow consumes it.
	bool bM_HasPendingQueuedMove = false;
	// Preserves whether the pending request should complete as reverse movement semantics.
	bool bM_IsReverse = false;
	// Completes the command that owns this movement after the controller reports success.
	EAbilityID M_CompletionAbility = EAbilityID::IdNoAbility;
	// Prevents delayed movement results from resolving a later execution of the same ability.
	uint64 M_CommandExecutionSerial = 0;
	// Optional request radius for movement commands that need a non-default acceptance radius.
	float M_GoalAcceptanceRadiusOverride = -1.f;
	// Captures whether request began from near-stationary speed to decide if nav-settle delay should be applied.
	bool bM_IsStationaryWhenQueued = true;
};

/**
 * Uses ATankMaster Logic for turrets and custom chaos physics for movement.
 * @note Setup in Child Blueprints:
 * @note 0) Call InitTrackedTank to setup bones on which forces are applied to move the tank.
 * @note 1) SetupTurret() to add a new turret to this vehicle.
 * @note 2) With the same turret reference call InitTurretsMaster() to set the owner to this tank.
 */
UCLASS()
class RTS_SURVIVAL_API ATrackedTankMaster : public ATankMaster, public IVehicleAIInterface,
                                            public IRTSUnit, public IExperienceInterface,
                                            public IDigInUnit
{
	GENERATED_BODY()

public:
	ATrackedTankMaster(const FObjectInitializer& ObjectInitializer);
	TObjectPtr<UDigInComponent> GetDigInComponent() const { return M_DigInComponent; }
	
	virtual ERTSAggroBehaviour GetEngagementStance() const override final;
	virtual void SetAggroStance(const ERTSAggroBehaviour NewStance) override final;

	virtual void UpdateVehicle_Implementation(
		float TargetAngle,
		float DestinationDistance,
		float DesiredSpeed,
		float CalculatedThrottle,
		float CurrentSpeed,
		float DeltaTime,
		float CalculatedBreak,
		float CalculatedSteering) override final;

	virtual void SetRTSOverlapEvasionEnabled(const bool bEnabled) override;
	
	// Used to determine if the vehicle can aggro by target acquistion component.
	bool DoesVehicleHaveAnyWeapons()const;

	virtual void OnFinishedPathFollowing() override;

	virtual USkeletalMeshComponent* GetTankMesh() const override final;

	inline UTrackPhysicsMovement* GetTrackPhysicsMovement() const { return TrackPhysicsMovement; }

	/**
	 * @brief Tells where and how a parked tank is meant to stand, so it can be sent back there after making room.
	 * Stopping a little short of a slot and being nudged since then must not become the new place.
	 * @param OutLocation The spot its last move went to when it arrived there, else where it stands now.
	 * @param OutRotation The facing it finished its last turn on, else how it is turned now.
	 */
	void GetParkedPose(FVector& OutLocation, FRotator& OutRotation) const;

	/** @return How this vehicle treats allied vehicles in its way; read by the avoidance and the overlap evasion. */
	virtual EAlliedVehiclePassage GetAlliedVehiclePassage() const;

	/**
	 * @return Radius around its move goal that a vehicle pushing allies aside keeps free of them, so they do not
	 * stand where it ends up; zero when only its path is cleared.
	 */
	virtual float GetAlliedPushGoalClearRadius() const { return 0.f; }

	/** @return Whether an ally that pushes others aside may order this vehicle off its path; not while dug in. */
	virtual bool GetCanBePushedAside() const;

	inline float GetInclineAngle() const { return TrackPhysicsMovement->GetInclineAngle(); }

	/** @param NewTurnRate The new turn rate used on this vehicle and the track physics movement. */
	virtual void UpgradeTurnRate(const float NewTurnRate) override final;

	/**
     * @brief Used when the unit is spawned to disable it unit it is ready to be used.
     * @param bSetDisabled Whether to enable or disable the unit.
     * @param TimeNotSelectable The time in seconds the unit is not selectable.
     * @param MoveTo The location to move to.
     */
	virtual void OnRTSUnitSpawned(
		const bool bSetDisabled,
		const float TimeNotSelectable = 0.f,
		const FVector MoveTo = FVector::ZeroVector) override;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Blueprintable, Category = "Experience")
	void BP_OnUnitLevelUp() const;

protected:
	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	virtual void UnitDies(const ERTSDeathType DeathType) override;

	virtual void Tick(float DeltaSeconds) override;

	// Called when the last command was terminated and the unit has no new commands in the queue.
	virtual void OnUnitIdleAndNoNewCommands() override;

	// Used at Init to determine how the chassis mesh interacts with the environment in terms of collision
	virtual void SetupChassisMeshCollision();

	virtual void ExecuteHarvestResourceCommand(ACPPResourceMaster* TargetResource) override;
	virtual void TerminateHarvestResourceCommand() override;

	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	URTSOverlapEvasionComponent* RTSOverlapEvasionComponent;
	bool EnsureRTSOverlapEvasionComponent() const;
	
	// ----------------- START EXPERIENCE INTERFACE -----------------
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	TObjectPtr<URTSExperienceComp> ExperienceComponent;

	UPROPERTY()
	TObjectPtr<UTankTargetAcquisition> M_TargetAcquisition;
	bool GetIsValidTargetAcquisition() const;
	

	virtual URTSExperienceComp* GetExperienceComponent() const override final;
	virtual void OnUnitLevelUp(const int32 Level, const EVeterancyIconSet IconSetUsed) override final;


	virtual void OnTankKilledAnyActor(AActor* KilledActor) override;

	// ----------------- END EXPERIENCE INTERFACE -----------------


	/**
	 * @brief Initialize the tracked tank.
	 * @param Mesh The chasis skeletal mesh of the tank.
	 * @param NewTrackForceMultiplier The multiplier for the track force greatly influences the vehicle speed. (default 2000)
	 * @param NewTurnRate The turn rate of the tank. (default 30)
	 * @param TrackedAnimBP The animation blueprint of the tank.
	 * @param TankCornerOffset The offset used in pathfinding to offset points from navmesh corners.
	 * @param TankMeshZOffset The offset of the tank mesh from the ground. (default 200)
	 * can be used on tanks where the physics assistance makes the mesh go through the ground.
	 * @param AngularDamping
	 * @param LinearDamping
	 * @param Energy
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category = "ReferenceCasts")
	void InitTrackedTank(
		USkeletalMeshComponent* Mesh,
		const float NewTrackForceMultiplier,
		const float NewTurnRate,
		UChassisAnimInstance* TrackedAnimBP,
		const float TankCornerOffset,
		const float TankMeshZOffset = 200.f, const float AngularDamping = 0, const float LinearDamping = 0, const int32 Energy
			= 0);

	// Exists both on the tank and the physics track movement component for quick accesss.
	UPROPERTY(BlueprintReadOnly, Category="Reference")
	UChassisAnimInstance* TankAnimationBP;

	/** Name of the trackmovement*/
	static FName TrackPhysicsMovementName;

	// Moves the tank using physics and line traces the landscape to adjust the forward vector used.
	UPROPERTY(Category = "Reference", VisibleDefaultsOnly, BlueprintReadOnly)
	UTrackPhysicsMovement* TrackPhysicsMovement;

	// =========================================
	// COMMAND INTERFACE OVERWRITES ------------
	// =========================================

	/** @copydoc ICommands::ExecuteMoveCommand */
	virtual void ExecuteMoveCommand(const FVector MoveToLocation) override;

	/** @copydoc ICommands::Fini */
	virtual void TerminateMoveCommand() override;

	/**
	 * @brief Clears stale deferred move state without braking when an immediate move replaces active movement.
	 * This lets tracked tanks accept rapid movement spam without resetting locomotion between destinations.
	 */
	virtual void TerminateMoveCommandForMovementReplacement() override;

	/** @brief Drives toward the tow target as part of the tow command; the AI controller reports back when it ends. */
	virtual bool TryStartTowApproachMove(const FVector& TowTargetLocation,
	                                     const float ApproachAcceptanceRadius) override;
	/** @brief Stops a tow approach drive, unless a queued move takes over driving. */
	virtual void TerminateTowActorCommand() override;

	virtual void ExecuteReverseCommand(const FVector ReverseToLocation) override;
	virtual void TerminateReverseCommand() override;

	/**
	 * @brief Clears stale deferred reverse state without braking when immediate movement replaces active reverse.
	 * This keeps the tank moving while the replacement command installs its own forward/reverse request.
	 */
	virtual void TerminateReverseCommandForMovementReplacement() override;


	virtual void StopMovement() override final;

	/** @copydoc ICommands::ExecuteAttackCommand */
	virtual void ExecuteAttackCommand(AActor* Target) override final;

	// Rotation logic is done in tank master, but here we set the correct track animation.
	virtual void ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand) override;
	// Sets the tracks back to idle.
	virtual void TerminateRotateTowardsCommand() override;
	virtual bool ApplyRotateTowardsStep(const float RemainingYawDegrees, const float DeltaSeconds) override;
	virtual void OnRotateTowardsFinished() override;
	virtual void OnCommandExecutionStarting(EAbilityID AbilityStarting) override;
	/** @param bDisabled Whether to enable or disable the unit. */
	UFUNCTION(BlueprintImplementableEvent, Category = "RTSUnitSpawning")
	void BP_OnRTSUnitSpawned(const bool bDisabled);

	virtual void ExecuteDigIn() override;
	virtual void TerminateDigIn() override;

	virtual void ExecuteBreakCover() override;
	virtual void TerminateBreakCover() override;

	virtual void ExecuteFireRockets() override;
	virtual void TerminateFireRockets() override;

	virtual void ExecuteCancelFireRockets() override;
	virtual void TerminateCancelFireRockets() override;

	// ============= Begin DigIn Unit Interface =============
	virtual void OnStartDigIn() override;
	virtual void OnDigInCompleted() override;
	virtual void OnBreakCoverCompleted() override;
	virtual void WallGotDestroyedForceBreakCover() override;
	// ============ End DigIn Unit Interface =============

	// =========================================
	// END COMMAND INTERFACE OVERWRITES ------------
	// =========================================

	// The skeletal mesh of this tank responsible for movement.
	// Exists on both the tank as well as the physics track movement component for quick access.
	UPROPERTY()
	USkeletalMeshComponent* ChassisMesh;

private:
	FTimerHandle M_OnSpawnedSelectionTimerHandle;

	// Solid box around the hull that keeps other vehicles out; welded to the chassis, so it moves as one body.
	UPROPERTY()
	TObjectPtr<UBoxComponent> M_HullBlocker;

	// Lets hulls slide along each other instead of grabbing and turning one another.
	UPROPERTY()
	TObjectPtr<UPhysicalMaterial> M_HullBlockerMaterial;

	void SetupHullBlocker();

	/**
	 * @brief Puts a vehicle that appears inside another hull on the nearest free ground; solid hulls would throw it up.
	 * @param MoveTo Where the vehicle is sent after it appears.
	 * @return Where to send it instead: the free spot when it was only meant to stay where it appeared.
	 */
	FVector MoveClearOfVehiclesWhenSpawned(const FVector& MoveTo);

	// How long a turn on the spot has been held up by another hull; decides when the tank works itself free.
	float M_RotateTowardsStallSeconds = 0.0f;
	float M_SecondsUntilRotateTowardsDisplacement = 0.0f;

	/**
	 * @brief Frees a tank whose turn on the spot is held up by a hull next to it, so the final facing is still reached.
	 * @param bIsTurnHeldUp Whether the tank is told to turn but hardly does.
	 * @param DeltaSeconds Frame time.
	 * @return Throttle that creeps the tank away from the hull in its way; zero while it turns freely.
	 */
	float GetThrottleToFreeHeldUpTurn(bool bIsTurnHeldUp, float DeltaSeconds);

	// The facing the tank finished its last turn on, kept until its next command; a shove must not undo it.
	float M_ParkedFacingYaw = 0.0f;
	bool bM_HasParkedFacing = false;
	FTimerHandle M_ParkedFacingTimerHandle;

	// The spot the tank last drove back to after being shoved off it, and how often; it does not keep trying forever.
	FVector M_ParkedReturnGoal = FVector::ZeroVector;
	int32 M_ParkedReturnAttempts = 0;

	/**
	 * @brief Sends a parked tank that was shoved off the spot its order put it on back to that spot.
	 * Only while nothing else has commanded it since it finished its turn there.
	 * @return True when the tank was sent back.
	 */
	bool TryReturnToParkedGoal();
	void RememberParkedFacing(float FacingYaw);
	void ForgetParkedFacing();
	void RestoreParkedFacingWhenTurnedAway();

	// Set at post init, may be left empty for nomadic vehicles that cannot digin.
	UPROPERTY()
	TObjectPtr<UDigInComponent> M_DigInComponent;

	UPROPERTY()
	TObjectPtr<UAudioComponent> M_EngineSoundComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UAudioComponent> M_EngineGasSoundComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Tank|Audio")
	TObjectPtr<USoundBase> M_EngineGasSound = nullptr;

	// Possibly set in derived blueprint for tanks that have the ability to fire rockets.
	UPROPERTY()
	TObjectPtr<UAttachedRockets> M_AttachedRockets;

	inline static const FName AudioSpeedParam = "Speed";
	inline static const FName EngineGasSoundComponentName = TEXT("EngineGasSound");
	inline static const FName EngineGasSoundLoopingParam = TEXT("Looping");

	UFUNCTION()
	void UpdateEngineSound() const;

	void PostInitializeComponents_SetupEngineSounds();
	void SetupEngineGasSoundComponent();
	void PlayEngineGasSoundEffect() const;

	UPROPERTY()
	FTimerHandle M_EngineSoundHandle;

	FTimerDelegate M_EngineSoundDel;

	void OnLevelUp_UpdateHealthbarRankIcon(const int32 Level, const EVeterancyIconSet IconSetUsed) const;

	bool EnsureValidExperienceComponent();

	// Needs access to correctly set types on RTS component so cannot be called in Post-init components!
	void BeginPlay_SetupExperienceComponent();

	void OnRTSUnitSpawned_SetEnabled(const float TimeNotSelectable, const FVector MoveTo);
	void OnRTSUnitSpawned_SetDisabled();

	void CheckFootPrintForOverlaps() const;

	bool GetIsValidDigInComponent() const;

	UPROPERTY()
	UTankEnergyComponent* M_TankEnergyComponent = nullptr;
	void OnInit_FindEnergyComponent(const int32 MyEnergy);
	
	bool GetIsValidTrackPhysicsMovement() const;
	bool GetIsValidAITankController();
	bool GetIsValidTankAnimationBP() const;
	bool GetIsValidEngineSoundComponent() const;
	bool GetIsValidEngineGasSoundComponent() const;
	bool GetIsValidEngineGasSound() const;

protected:
	/**
	 * @brief Preserves stable path quality for first move while preventing repeated chain delays that reintroduce COL stutter.
	 * @param TargetLocation Requested move destination.
	 * @param bIsReverse Whether the queued request should keep reverse path-follow mode.
	 * @note A stationary start uses nav-settle delay, but chained movement while already moving skips full delay.
	 */
	void ExecuteTrackedMoveWithNavSettleDelay(const FVector& TargetLocation, const bool bIsReverse);

	/**
	 * @brief Uses tracked tank move issuing for commands that need custom completion or acceptance.
	 * @param TargetLocation Requested move destination.
	 * @param bIsReverse Whether the queued request should keep reverse path-follow mode.
	 * @param CompletionAbility Command completed when path following succeeds.
	 * @param GoalAcceptanceRadiusOverride Acceptance radius used instead of the path component default.
	 */
	void ExecuteTrackedMoveWithNavSettleDelayForAbility(
		const FVector& TargetLocation,
		const bool bIsReverse,
		const EAbilityID CompletionAbility,
		const float GoalAcceptanceRadiusOverride = -1.f);

	/**
	 * @brief Keeps timer callback logic isolated so deferred issue always reuses the same request state gate.
	 * @note This avoids duplicate direct issue paths when rapid command spam replaces pending requests.
	 */
	void ExecuteTrackedMoveWithNavSettleDelay_Deferred();

	/**
	 * @brief Executes the already-normalized move request so queue completion and reverse mode stay in sync.
	 * @param TargetLocation Destination that passed delay/coalescing policy.
	 * @param bIsReverse Whether completion should map to reverse movement semantics.
	 * @param CompletionAbility Queue ability that owns the movement phase.
	 * @param CommandExecutionSerial Token captured when the owning command execution started.
	 * @param GoalAcceptanceRadiusOverride Acceptance radius used instead of the path component default.
	 */
	void ExecuteTrackedMoveNow(
		const FVector& TargetLocation,
		const bool bIsReverse,
		const EAbilityID CompletionAbility,
		uint64 CommandExecutionSerial,
		const float GoalAcceptanceRadiusOverride);

	/**
	 * @brief Resets reverse path-following only during full reverse termination.
	 * Replacement moves skip this because the new request will immediately set the correct reverse mode.
	 */
	void ResetTrackedReversePathFollowing();

	/**
	 * @brief Centralizes full tracked movement shutdown so replacement termination can avoid braking.
	 * Used when no movement command follows and the tank really should stop.
	 */
	void FullyStopTrackedMovementCommand();
	
	virtual void OnCancelMovementToGetInRangeOfTurret() final;

	void CancelPendingTrackedMove();

	/**
	 * @brief Defers pre-controller failures so existing command-start collision sequencing can finish safely.
	 * @param FailedAbility Queue ability whose movement could not be issued.
	 * @param CommandExecutionSerial Token captured when the owning command execution started.
	 */
	void DeferTrackedMovementRequestFailure(EAbilityID FailedAbility, uint64 CommandExecutionSerial);

	void StartMovementCommandInvariantMonitoring();
	void StopMovementCommandInvariantMonitoring();
	void CheckMovementCommandInvariant();
	void ResetOrphanedMovementInvariantGracePeriod();
	/**
	 * @brief Terminates movement only after the request/pending-work invariant has remained broken past its grace period.
	 * @param CommandData Command state whose current execution is being repaired.
	 * @param TankController Controller used to discard any stale request ownership.
	 */
	void RecoverOrphanedMovementCommand(
		UCommandData* CommandData,
		AAITankMaster* TankController);

	void HandleVoiceLineOnLevelUp() const; 

	// Aggregates the currently requested move so stationary starts can wait for nav settle, while chains can repath instantly.
	UPROPERTY()
	FTrackedTankQueuedMoveState M_QueuedMoveState;

	UPROPERTY()
	FTimerHandle M_DeferredTrackedMoveHandle;

	FTimerHandle M_MovementCommandInvariantTimerHandle;
	double M_OrphanedMovementInvariantStartTimeSeconds = -1.0;

};
