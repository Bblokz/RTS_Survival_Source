// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoTypes.h"
#include "CargoSquad.generated.h"

class UCargo;
class ASquadController;
class ASquadUnit;

/**
 * @brief Per-squad cargo manager. Encapsulates all cargo in/out logic so the controller stays slim.
 * 
 * @note Init: call in BeginPlay of the owning squad controller to pass controller reference.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UCargoSquad : public UActorComponent
{
	GENERATED_BODY()

public:
	UCargoSquad();
	/**
	 * @brief Called when all units in the owning squad have died while inside cargo.
	 *
	 * Frees cargo slot/vacancy and resets local cargo state without moving units,
	 * so ragdolls remain unaffected.
	 */
	void OnSquadCompletelyDiedInside();
	/**
	 * @brief Initialize with owning controller.
	 * @param InController Owning squad controller.
	 */
	void Init(ASquadController* InController);

	/** @return True if the squad is currently inside cargo. */
	bool GetIsInsideCargo() const { return M_SquadCargoState == ESquadCargoState::Inside; }

	void OnUnitDiedWhileInside(ASquadUnit* Unit);

	/**
	 * @brief Validate a new EnterCargo request.
	 * @param Target Must be an actor with a valid UCargo component.
	 * @return true if valid, false otherwise (also logs).
	 */
	bool IsEnterCargoRequestValid(AActor* Target) const;

	/**
	 * @brief Entry point used by the controller’s ExecuteEnterCargoCommand.
	 *        Will move to closest entrance (as the EnterCargo ability) and when all units arrive,
	 *        UCargoSquad::OnEnterCargo_MoveToEntranceCompleted must be invoked by the controller.
	 */
	void ExecuteEnterCargo(AActor* TargetCargoActor);

	/**
	 * @brief Called by the controller when all units completed the EnterCargo move (ability finished moving).
	 *        Requests capacity & teleports/attaches the units to their assigned sockets, disables movement,
	 *        and swaps the Enter/Exit abilities. Finally, it finishes the EnterCargo command.
	 */
	void OnEnterCargo_MoveToEntranceCompleted();

	/**
	 * @brief Immediately exit cargo (teleport near closest entrance) without queuing an ExitCargo command.
	 *        Used by CheckCargoState when another ability requires being outside.
	 * @param bSwapAbilities Whether to swap Exit->Enter in the UI.
	 */
	void ExitCargoImmediate(bool bSwapAbilities);

	/**
	 * @brief Check cargo state when a new ability will execute. If this ability must be outside,
	 *        and we’re inside, instantly exit (no queue finish). Also takes care of UI ability swap.
	 * @param NewAbility The ability that is about to execute.
	 */
	void CheckCargoState(EAbilityID NewAbility);

	/**
	 * @brief Explicit ExitCargo ability (UI click). Will exit immediately and mark the ExitCargo command done.
	 *        The controller should wire this to ExecuteExitCargoCommand().
	 */
	void ExecuteExitCargo_Explicit();

	
	/**
	 * @brief Re-seat a single unit inside the current cargo to a new socket.
	 *
	 * Called by the UCargo component’s seat-reshuffle timer when an empty seat becomes
	 * available. Keeps the internal seat-assignment map in sync and re-attaches the unit.
	 * @param CargoComponent Cargo component requesting the seat change.
	 * @param UnitToMove     Squad unit that should move to the new cargo seat.
	 * @param NewSocketName  Target cargo socket name on the cargo mesh.
	 */
	void HandleSeatReassignmentFromCargo(UCargo* CargoComponent, ASquadUnit* UnitToMove,
	                                     const FName& NewSocketName);

private:
	/** Owning controller (weak). */
	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_OwningController;

	/** Currently occupied cargo component (weak). */
	UPROPERTY()
	TWeakObjectPtr<UCargo> M_CurrentCargo;

	/** Current cargo state. */
	UPROPERTY()
	ESquadCargoState M_SquadCargoState = ESquadCargoState::None;

	/** Mapping of squad units to their assigned seat socket while inside. */
	UPROPERTY()
	TMap<TWeakObjectPtr<ASquadUnit>, FName> M_UnitSeatAssignments;

	/** Target cargo actor while the EnterCargo command is in progress. */
	UPROPERTY()
	TWeakObjectPtr<AActor> M_PendingTargetCargoActor;

private:
	// --- Helpers ---
	bool GetIsValidController() const;
	bool GetIsValidCurrentCargo() const;

	/** @return Ability types that require being outside cargo. */
	static bool AbilityRequiresOutside(const EAbilityID Ability);

	/** Disable movement (but allow rotation/anim) and attach the unit to seat socket. */
	void AttachUnitAndDisableMovement(ASquadUnit* Unit, UCargo* Cargo, const FName& SocketName) const;

	/** Re-enable movement and detach unit; teleport near entrance. */
	void DetachUnitEnableMovementAndPlaceNearEntrance(ASquadUnit* Unit, UCargo* Cargo) const;

	/** Utility to attempt an ability swap and error-report if it fails. */
	void TrySwapAbilities(const EAbilityID OldAbility, const EAbilityID NewAbility) const;

	void OnEnterCargo_EarlyTerminate();

	
	/** Validates there is a pending cargo actor and returns its UCargo component. */
	bool ValidatePendingCargoAndResolve(class UCargo*& OutCargo) const;

	/**
	 * Computes current closest entrance and checks if the squad should re-path to it.
	 * Uses a cargo-specific acceptance radius and the **minimum** distance of any unit.
	 * @return true if re-pathing is needed; also returns EntranceNow via OutEntranceNow.
	 */
	bool ShouldRepathToEntrance(class UCargo* TargetCargo, FVector& OutEntranceNow) const;

	/**
	 * Queries seats from cargo and attaches units if possible. Fills M_UnitSeatAssignments.
	 * @return true if seats were assigned and units attached; false if no vacancy.
	 */
	bool OnPhysicallyEnteringCargo(class UCargo* TargetCargo);

	/** Commits state after boarding (sets current cargo, swaps abilities, hides nav abilities, etc.) */
	void FinalizeEnterCargo(class UCargo* TargetCargo);

	// --- Ability suspension for cargo ---
    /** Original indices of temporarily removed nav abilities (stored when we board). */
    UPROPERTY()
    TMap<EAbilityID, int32> M_SuspendedNavAbilityIndices;
    
    /** Remove movement/patrol abilities regardless of current index. */
    void SuspendNavAbilities_WhileInside();
    
    /** Restore previously removed movement/patrol abilities at their original indices when possible. */
    void RestoreNavAbilities_AfterExit();

	
    /** Last entrance location we asked the squad to move to for EnterCargo. */
    UPROPERTY()
    FVector M_LastRequestedCargoEntrance = FVector::ZeroVector;

    /** Safety counter to prevent infinite re-path loops when units cannot reach the entrance. */
    UPROPERTY()
    int32 M_RepathToEntranceAttempts = 0;
};
