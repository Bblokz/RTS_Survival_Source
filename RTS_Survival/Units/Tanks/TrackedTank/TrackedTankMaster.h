// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Interfaces/RTSInterface/RTSUnit.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInUnit/DigInUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/FRTSOverlapEvasion/RTSOverlapEvasionComponent.h"
#include "RTS_Survival/Units/Tanks/VehicleAI/Utils/VehicleAIInterface.h"
#include "TrackPhysicsMovementComp/TrackPhysicsMovement.h"
#include "TrackedTankMaster.generated.h"

class UAttachedRockets;
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

	virtual void OnFinishedPathFollowing() override;

	virtual USkeletalMeshComponent* GetTankMesh() const override final;

	inline UTrackPhysicsMovement* GetTrackPhysicsMovement() const { return TrackPhysicsMovement; }

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
	virtual void ApplyRotateTowardsStep(const float TurnAmountDegrees, const float DeltaSeconds) override;
	virtual void OnRotateTowardsFinished() override;
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

	// Set at post init, may be left empty for nomadic vehicles that cannot digin.
	UPROPERTY()
	TObjectPtr<UDigInComponent> M_DigInComponent;

	UPROPERTY()
	UAudioComponent* M_EngineSoundComponent;

	// Possibly set in derived blueprint for tanks that have the ability to fire rockets.
	UPROPERTY()
	TObjectPtr<UAttachedRockets> M_AttachedRockets;

	inline static const FName AudioSpeedParam = "Speed";

	UFUNCTION()
	void UpdateEngineSound() const;

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

	/**
	 * @brief Preserves stable path quality for first move while preventing repeated chain delays that reintroduce COL stutter.
	 * @param TargetLocation Requested move destination.
	 * @param bIsReverse Whether the queued request should keep reverse path-follow mode.
	 * @note A stationary start uses nav-settle delay, but chained movement while already moving skips full delay.
	 */
	void ExecuteTrackedMoveWithNavSettleDelay(const FVector& TargetLocation, const bool bIsReverse);

	/**
	 * @brief Keeps timer callback logic isolated so deferred issue always reuses the same request state gate.
	 * @note This avoids duplicate direct issue paths when rapid command spam replaces pending requests.
	 */
	void ExecuteTrackedMoveWithNavSettleDelay_Deferred();

	/**
	 * @brief Executes the already-normalized move request so queue completion and reverse mode stay in sync.
	 * @param TargetLocation Destination that passed delay/coalescing policy.
	 * @param bIsReverse Whether completion should map to reverse movement semantics.
	 * @note Always set queued movement completion ability before issuing controller move request.
	 */
	void ExecuteTrackedMoveNow(const FVector& TargetLocation, const bool bIsReverse);

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

	void CancelPendingTrackedMove();

	void HandleVoiceLineOnLevelUp() const; 

	// Aggregates the currently requested move so stationary starts can wait for nav settle, while chains can repath instantly.
	UPROPERTY()
	FTrackedTankQueuedMoveState M_QueuedMoveState;

	UPROPERTY()
	FTimerHandle M_DeferredTrackedMoveHandle;

};
