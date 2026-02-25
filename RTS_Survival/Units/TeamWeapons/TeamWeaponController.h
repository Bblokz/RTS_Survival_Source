// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Weapons/Turret/TurretOwner/TurretOwner.h"
#include "TeamWeaponMover.h"
#include "TeamWeaponState/TeamWeaponState.h"
#include "TeamWeaponController.generated.h"

class ATeamWeapon;
class UAnimatedTextWidgetPoolManager;
class AActor;


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
struct FTeamWeaponPostDeployPackAction
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
	}

	/**
	 * @brief Initializes the action payload so post-pack execution can be deterministic.
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
	                    const bool bInHasRotation, const FRotator& InTargetRotation, AActor* InTargetActor)
	{
		Reset();
		M_AbilityId = AbilityId;
		bM_HasLocation = bInHasLocation;
		bM_HasRotation = bInHasRotation;
		bM_HasTargetActor = InTargetActor != nullptr;
		M_TargetLocation = InTargetLocation;
		M_TargetRotation = InTargetRotation;
		M_TargetActor = InTargetActor;
	}

	EAbilityID GetAbilityId() const { return M_AbilityId; }
	bool GetHasAction() const { return M_AbilityId != EAbilityID::IdNoAbility; }
	bool GetHasLocation() const { return bM_HasLocation; }
	bool GetHasRotation() const { return bM_HasRotation; }
	bool GetHasTargetActor() const { return bM_HasTargetActor; }
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
};

/**
 * @brief Squad controller that coordinates team weapon crews and keeps their weapon synchronized during moves.
 * Ensures the weapon follows the crew spacing so the emplacement stays aligned with its operators.
 */
UCLASS()
class RTS_SURVIVAL_API ATeamWeaponController : public ASquadController, public ITurretOwner
{
	GENERATED_BODY()

public:
	ATeamWeaponController();

protected:
	virtual void BeginPlay() override;
	virtual void OnAllSquadUnitsLoaded() override;
	virtual void ExecuteMoveCommand(const FVector MoveToLocation) override;
	virtual void ExecutePatrolCommand(const FVector PatrolToLocation) override;
	virtual void ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand) override;
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
	void HandleMoverArrived();
	void HandleMoverFailed(const FString& FailureReason);
	void SetTeamWeaponState(const ETeamWeaponState NewState);
	bool GetIsValidTeamWeapon() const;
	bool GetIsValidTeamWeaponMover() const;
	bool GetIsValidAnimatedTextWidgetPoolManager() const;

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

	void SetPostDeployPackActionForMove(const FVector MoveToLocation);
	void TryIssuePostDeployPackAction();
	void IssuePostDeployPackAction();
	void UpdateCrewMoveOffsets();
	bool TryGetCrewMemberOffset(const ASquadUnit* SquadUnit, FVector& OutOffset) const;
	bool GetIsCrewOperator(const ASquadUnit* SquadUnit) const;
	void ApplyCrewOffsetToPath(FNavPathSharedPtr& UnitPath, const FVector& CrewOffset) const;
	void ApplyNonCrewOffsetToPath(FNavPathSharedPtr& UnitPath, const FVector& UnitOffset) const;
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

	UPROPERTY(EditDefaultsOnly, Category = "TeamWeapon")
	TSubclassOf<ATeamWeapon> TeamWeaponClass;

	UPROPERTY()
	TObjectPtr<ATeamWeapon> M_TeamWeapon;

	UPROPERTY()
	TWeakObjectPtr<UTeamWeaponMover> M_TeamWeaponMover;

	UPROPERTY()
	FTeamWeaponCrewAssignment M_CrewAssignment;

	UPROPERTY()
	ETeamWeaponState M_TeamWeaponState = ETeamWeaponState::Uninitialized;

	UPROPERTY()
	TWeakObjectPtr<UAnimatedTextWidgetPoolManager> M_AnimatedTextWidgetPoolManager;

	UPROPERTY()
	TArray<FTeamWeaponCrewMemberOffset> M_CrewMoveOffsets;

	// Pending command data that should execute after packing/deploying completes.
	UPROPERTY()
	FTeamWeaponPostDeployPackAction M_PostDeployPackAction;

	UPROPERTY()
	FTimerHandle M_DeployTimer;
};
