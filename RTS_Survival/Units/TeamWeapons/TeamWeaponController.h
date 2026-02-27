// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInUnit/DigInUnit.h"
#include "RTS_Survival/Weapons/Turret/TurretOwner/TurretOwner.h"
#include "TeamWeaponMover.h"
#include "TeamWeaponState/TeamWeaponState.h"
#include "TeamWeaponController.generated.h"

class ATeamWeapon;
class UAnimatedTextWidgetPoolManager;
class AActor;
class UCrewPosition;
class UDigInComponent;


USTRUCT()
struct FTeamWeaponCrewAssignment
{
	GENERATED_BODY()

	void Reset()
	{
		M_Operators.Reset();
		M_Guards.Reset();
		M_RequiredOperators = 0;
	}

	bool GetHasEnoughOperators() const
	{
		return M_RequiredOperators > 0 && M_Operators.Num() >= M_RequiredOperators;
	}

	int32 M_RequiredOperators = 0;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadUnit>> M_Operators;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadUnit>> M_Guards;
};

USTRUCT()
struct FTeamWeaponDeferredAction
{
	GENERATED_BODY()

	void Reset()
	{
		M_AbilityId = EAbilityID::IdNoAbility;
		M_TargetLocation = FVector::ZeroVector;
		M_TargetRotation = FRotator::ZeroRotator;
		M_TargetActor = nullptr;
		bM_HasLocation = false;
		bM_HasRotation = false;
		bM_HasTargetActor = false;
		bM_ShouldTriggerDoneExecuting = true;
	}

	/**
	 * @brief Initializes the action payload so deferred execution can be deterministic.
	 * Captures only the data needed for the intended command and clears unused fields.
	 *
	 * @param AbilityId Command to execute after packing or deploying completes.
	 * @param bInHasLocation True when the command requires a location.
	 * @param InTargetLocation Target location for movement-like commands.
	 * @param bInHasRotation True when the command requires a rotation.
	 * @param InTargetRotation Desired facing for rotation-like commands.
	 * @param InTargetActor Optional actor target for actor-based commands.
	 */
	void InitForCommand(const EAbilityID AbilityId, const bool bInHasLocation, const FVector& InTargetLocation,
	                    const bool bInHasRotation, const FRotator& InTargetRotation, AActor* InTargetActor,
	                    const bool bInShouldTriggerDoneExecuting = true)
	{
		Reset();
		M_AbilityId = AbilityId;
		bM_HasLocation = bInHasLocation;
		bM_HasRotation = bInHasRotation;
		bM_HasTargetActor = InTargetActor != nullptr;
		M_TargetLocation = InTargetLocation;
		M_TargetRotation = InTargetRotation;
		M_TargetActor = InTargetActor;
		bM_ShouldTriggerDoneExecuting = bInShouldTriggerDoneExecuting;
	}

	EAbilityID GetAbilityId() const { return M_AbilityId; }
	bool GetHasAction() const { return M_AbilityId != EAbilityID::IdNoAbility; }
	bool GetHasLocation() const { return bM_HasLocation; }
	bool GetHasRotation() const { return bM_HasRotation; }
	bool GetHasTargetActor() const { return bM_HasTargetActor; }
	bool GetShouldTriggerDoneExecuting() const { return bM_ShouldTriggerDoneExecuting; }
	const FVector& GetTargetLocation() const { return M_TargetLocation; }
	const FRotator& GetTargetRotation() const { return M_TargetRotation; }
	TWeakObjectPtr<AActor> GetTargetActor() const { return M_TargetActor; }

private:
	EAbilityID M_AbilityId = EAbilityID::IdNoAbility;
	FVector M_TargetLocation = FVector::ZeroVector;
	FRotator M_TargetRotation = FRotator::ZeroRotator;
	TWeakObjectPtr<AActor> M_TargetActor;
	bool bM_HasLocation = false;
	bool bM_HasRotation = false;
	bool bM_HasTargetActor = false;
	bool bM_ShouldTriggerDoneExecuting = true;
};


USTRUCT()
struct FTeamWeaponRotationRequest
{
	GENERATED_BODY()

	void Reset()
	{
		M_TargetRotation = FRotator::ZeroRotator;
		bM_IsActive = false;
		bM_ShouldTriggerDoneExecuting = false;
		M_CompletionAbilityId = EAbilityID::IdNoAbility;
	}

	FRotator M_TargetRotation = FRotator::ZeroRotator;
	bool bM_IsActive = false;
	bool bM_ShouldTriggerDoneExecuting = false;
	EAbilityID M_CompletionAbilityId = EAbilityID::IdNoAbility;
};

/**
 * @brief Squad controller that coordinates team weapon crews and keeps their weapon synchronized during moves.
 * Ensures the weapon follows the crew spacing so the emplacement stays aligned with its operators.
 */
UCLASS()
class RTS_SURVIVAL_API ATeamWeaponController : public ASquadController, public ITurretOwner, public IDigInUnit
{
	GENERATED_BODY()

public:
	ATeamWeaponController();
	bool RequestInternalRotateTowards(const FRotator& DesiredRotation);

	virtual TArray<UWeaponState*> GetWeaponsOfSquad() override;

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnAllSquadUnitsLoaded() override;
	virtual void ExecuteMoveCommand(const FVector MoveToLocation) override;
	virtual void TerminateMoveCommand() override;
	virtual void ExecutePatrolCommand(const FVector PatrolToLocation) override;
	virtual void ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand) override;
	virtual void TerminateRotateTowardsCommand() override;
	virtual void ExecuteAttackCommand(AActor* TargetActor) override;
	virtual void TerminateAttackCommand() override;
	virtual void ExecuteDigIn() override;
	virtual void TerminateDigIn() override;
	virtual void ExecuteBreakCover() override;
	virtual void TerminateBreakCover() override;
	virtual void OnUnitIdleAndNoNewCommands() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void UnitInSquadDied(ASquadUnit* UnitDied, bool bUnitSelected, ERTSDeathType DeathType) override;
	virtual void OnSquadUnitCommandComplete(EAbilityID CompletedAbilityID) override;

	/**
	 * @brief Assigns paths while preserving operator spacing so the weapon shadow stays stable.
	 * Avoids random offsets for crew so the weapon remains centered on their formation.
	 *
	 * @details Operators receive a path offset based on their starting spacing, keeping their
	 *          relative distances intact while the rest of the squad uses standard spread offsets.
	 *
	 * @param MoveToLocation Destination used for path creation.
	 * @param SquadPath Shared base path to clone and offset per unit.
	 * @return Pathfinding error result for the assignment step.
	 */
	virtual ESquadPathFindingError GeneratePaths_Assign(const FVector& MoveToLocation,
	                                                    const FNavPathSharedPtr& SquadPath) override;

	/** @brief Begins packing so the controller can issue a post-pack action once ready. */
	virtual void StartPacking();

	/** @brief Begins deploying so the weapon can return to a firing-ready state. */
	virtual void StartDeploying();

private:
	void SpawnTeamWeapon();
	void AssignCrewToTeamWeapon();
	void HandlePushedMoverArrived();
	void HandlePushedMoverFailed(const FString& FailureReason);
	void SetTeamWeaponState(const ETeamWeaponState NewState);
	void TryAbandonTeamWeaponForInsufficientCrew();
	void AbandonTeamWeapon();
	bool GetIsGameShuttingDown() const;
	bool GetIsValidTeamWeapon() const;
	bool GetIsPushedWeaponLeadsMovementType() const;
	bool GetIsValidTeamWeaponMover() const;
	bool GetIsValidAnimatedTextWidgetPoolManager() const;
	bool GetIsValidDigInComponent() const;

	void BeginPlay_InitAnimatedTextWidgetPoolManager();

	/**
	 * @brief Begins the crew-led movement flow after any packing delay is resolved.
	 * Ensures the mover follows operators while the squad walks along their generated paths.
	 *
	 * @details Generates operator offsets, populates the mover crew list, issues squad movement,
	 *          and activates the mover so it can shadow the crew until arrival.
	 *
	 * @param MoveToLocation Destination for the squad and team weapon.
	 */
	void StartMoveWithCrew(const FVector MoveToLocation);
	void StartMoveWithPushedWeapon(const FVector MoveToLocation);
	void ExecuteSquadMoveAlongAssignedPaths(const EAbilityID AbilityId) const;
	void ApplyPushedMoveSpeedOverrideToSquad();
	void RestorePushedMoveSpeedOverride();

	/**
	 * @brief Starts the deployment timer so the weapon returns to a firing-ready state.
	 * Provides player feedback during the wait using animated text.
	 *
	 * @details Uses the configured deployment time to transition from packed to deployed,
	 *          updating the team weapon state only when the timer completes.
	 */
	void StartDeployingTeamWeapon();

	/** @brief Cached timer callback that finalizes the packing sequence. */
	void HandlePackingTimerFinished();

	/** @brief Cached timer callback that finalizes the deployment sequence. */
	void HandleDeployingTimerFinished();

	void SetPostPackActionForMove(const FVector MoveToLocation);
	bool GetHasPendingMovePostPackAction() const;

	void SetPostPackActionForRotate(const FRotator& DesiredRotation, const EAbilityID AbilityId,
	                                 const bool bShouldTriggerDoneExecuting);
	void SetPostDeployActionForAttack(AActor* TargetActor);
	void StartRotationRequest(const FRotator& DesiredRotation, const bool bShouldTriggerDoneExecuting,
	                         const EAbilityID CompletionAbilityId);
	void TickRotationRequest(const float DeltaSeconds);
	void RotateControllerAndTeamWeapon(const float StepYaw);
	void FinishRotationRequest();
	void AttachCrewForRotation();
	void DetachCrewAfterRotation();
	void SnapOperatorsToCrewPositionsDuringRotation();
	bool TryGetLandscapeTeleportLocationForCrewPosition(const ASquadUnit* SquadUnit,
	                                                    const FVector& CrewPositionLocation,
	                                                    FVector& OutTeleportLocation) const;
	void MoveGuardsToRandomGuardPositions() const;
	bool TryGetRandomGuardLocation(FVector& OutGuardLocation) const;
	FVector GetMoveLocationWithinTurretRange(const FVector& TargetLocation, const ACPPTurretsMaster* CallingTurret) const;
	void TryIssuePostPackAction();
	void IssuePostPackAction();
	void IssuePostPackAction_Move();
	void IssuePostPackAction_Rotate();
	void TryIssuePostDeployAction();
	void IssuePostDeployAction();
	void IssuePostDeployAction_Attack();
	void UpdateCrewMoveOffsets();
	bool TryGetCrewMemberOffset(const ASquadUnit* SquadUnit, FVector& OutOffset) const;
	bool GetIsCrewOperator(const ASquadUnit* SquadUnit) const;
	void ApplyCrewOffsetToPath(FNavPathSharedPtr& UnitPath, const FVector& CrewOffset) const;
	void ApplyNonCrewOffsetToPath(FNavPathSharedPtr& UnitPath, const FVector& UnitOffset) const;
	bool TryGetCrewPositionsSorted(TArray<UCrewPosition*>& OutCrewPositions) const;
	void IssueMoveCrewToPositions();
	void ShowDeployingAnimatedText() const;
	void ShowPackingAnimatedText() const;

	// ---- ITurretOwner ----
	virtual int GetOwningPlayer() override;
	virtual void OnTurretOutOfRange(const FVector TargetLocation, ACPPTurretsMaster* CallingTurret) override;
	virtual void OnTurretInRange(ACPPTurretsMaster* CallingTurret) override;
	virtual void OnMountedWeaponTargetDestroyed(
		ACPPTurretsMaster* CallingTurret,
		UHullWeaponComponent* CallingHullWeapon,
		AActor* DestroyedActor,
		const bool bWasDestroyedByOwnWeapons) override;
	virtual void OnFireWeapon(ACPPTurretsMaster* CallingTurret) override;
	virtual void OnProjectileHit(const bool bBounced) override;
	virtual FRotator GetOwnerRotation() const override;
	// ---- End ITurretOwner ----

	// ---- IDigInUnit ----
	virtual void OnStartDigIn() override;
	virtual void OnDigInCompleted() override;
	virtual void OnBreakCoverCompleted() override;
	virtual void WallGotDestroyedForceBreakCover() override;
	// ---- End IDigInUnit ----

	UPROPERTY(EditDefaultsOnly, Category = "TeamWeapon")
	TSubclassOf<ATeamWeapon> TeamWeaponClass;

	UPROPERTY(EditDefaultsOnly, Category = "TeamWeapon|Guard")
	float M_GuardDistance = 350.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TeamWeapon|Guard")
	FVector2D M_GuardMinMaxAngle = FVector2D(-150.0f, -30.0f);

	UPROPERTY()
	TObjectPtr<ATeamWeapon> M_TeamWeapon;

	UPROPERTY()
	TWeakObjectPtr<UTeamWeaponMover> M_TeamWeaponMover;

	UPROPERTY()
	TMap<TObjectPtr<ASquadUnit>, float> M_PushedMoveOriginalUnitSpeeds;

	UPROPERTY()
	FTeamWeaponCrewAssignment M_CrewAssignment;

	UPROPERTY()
	ETeamWeaponState M_TeamWeaponState = ETeamWeaponState::Uninitialized;

	UPROPERTY()
	TWeakObjectPtr<UAnimatedTextWidgetPoolManager> M_AnimatedTextWidgetPoolManager;

	UPROPERTY()
	TArray<FTeamWeaponCrewMemberOffset> M_CrewMoveOffsets;

	// Pending command data that should execute after packing completes.
	UPROPERTY()
	FTeamWeaponDeferredAction M_PostPackAction;

	// Pending command data that should execute after deploying completes.
	UPROPERTY()
	FTeamWeaponDeferredAction M_PostDeployAction;

	UPROPERTY()
	FTimerHandle M_DeployTimer;


	UPROPERTY()
	FTeamWeaponRotationRequest M_RotationRequest;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_SpecificEngageTarget;


	UPROPERTY()
	bool bM_AreCrewAttachedForRotation = false;

	// Prevents repeatedly issuing identical move orders while staying in the same deploy cycle.
	UPROPERTY()
	bool bM_HasIssuedCrewPositionMovesForCurrentDeploy = false;

	// Target location used to push guard final positions slightly farther forward during engage repositioning.
	UPROPERTY()
	FVector M_GuardEngageFlowTargetLocation = FVector::ZeroVector;

	UPROPERTY()
	bool bM_HasGuardEngageFlowTargetLocation = false;

	UPROPERTY()
	bool bM_IsTeamWeaponAbandoned = false;

	UPROPERTY()
	TObjectPtr<UDigInComponent> M_DigInComponent;

	UPROPERTY()
	bool bM_ShouldExecuteDigInAfterDeploy = false;

	UPROPERTY()
	bool bM_IsShuttingDown = false;
};
