// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "CargoSquad.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoTypes.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"

UCargoSquad::UCargoSquad()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCargoSquad::OnSquadCompletelyDiedInside()
{
	if (not GetIsInsideCargo() || not GetIsValidCurrentCargo() || not GetIsValidController())
	{
		return;
	}

	UCargo* const Cargo = M_CurrentCargo.Get();
	if (not IsValid(Cargo))
	{
		return;
	}

	ASquadController* const SquadController = M_OwningController.Get();
	if (not IsValid(SquadController))
	{
		return;
	}

	// Let the cargo clear its vacancy and UI slot for this squad without double-adjusting seats.
	Cargo->OnSquadCompletelyDiedInside(SquadController);

	// Locally reset state; this squad no longer occupies any cargo.
	M_CurrentCargo = nullptr;
	M_SquadCargoState = ESquadCargoState::None;
	M_UnitSeatAssignments.Empty();
}


void UCargoSquad::Init(ASquadController* InController)
{
	M_OwningController = InController;
	if (not GetIsValidController())
	{
		return;
	}
	// Initial outside state.
	M_SquadCargoState = ESquadCargoState::Free;
}

bool UCargoSquad::GetIsValidController() const
{
	if (M_OwningController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(GetOwner(), "M_OwningController",
	                                                      "UCargoSquad::GetIsValidController");
	return false;
}

bool UCargoSquad::GetIsValidCurrentCargo() const
{
	if (M_CurrentCargo.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(GetOwner(), "M_CurrentCargo",
	                                                      "UCargoSquad::GetIsValidCurrentCargo");
	return false;
}

void UCargoSquad::OnUnitDiedWhileInside(ASquadUnit* Unit)
{
	if (not GetIsInsideCargo() || not GetIsValidCurrentCargo() || not GetIsValidController())
	{
		return;
	}

	// Forget this unit’s seat assignment (if we tracked it).
	M_UnitSeatAssignments.Remove(Unit);

	// Tell the cargo to reduce the seat count by 1, but keep the slot occupied.
	if (UCargo* Cargo = M_CurrentCargo.Get())
	{
		Cargo->AdjustSeatsForSquad(M_OwningController.Get(), /*SeatsDelta=*/-1);
	}
}


bool UCargoSquad::IsEnterCargoRequestValid(AActor* Target) const
{
	if (not Target)
	{
		RTSFunctionLibrary::ReportError("UCargoSquad::IsEnterCargoRequestValid -> Target actor is null");
		return false;
	}
	if (not Target->FindComponentByClass<UCargo>())
	{
		RTSFunctionLibrary::ReportError("UCargoSquad::IsEnterCargoRequestValid -> Target has no UCargo component: " +
			Target->GetName());
		return false;
	}
	return true;
}

void UCargoSquad::ExecuteEnterCargo(AActor* TargetCargoActor)
{
    if (not GetIsValidController())
    {
        return;
    }
    if (not IsEnterCargoRequestValid(TargetCargoActor))
    {
        RTSFunctionLibrary::ReportError("ExecuteEnterCargo aborted due to invalid target cargo actor.");
        M_OwningController->DoneExecutingCommand(EAbilityID::IdEnterCargo);
        return;
    }

    UCargo* TargetCargo = TargetCargoActor->FindComponentByClass<UCargo>();
    if (not IsValid(TargetCargo))
    {
        RTSFunctionLibrary::ReportError("ExecuteEnterCargo aborted -> Target has no UCargo component.");
        M_OwningController->DoneExecutingCommand(EAbilityID::IdEnterCargo);
        return;
    }

    // Enter while already inside?
    if (GetIsInsideCargo())
    {
        if (M_CurrentCargo.Get() == TargetCargo)
        {
            M_OwningController->DoneExecutingCommand(EAbilityID::IdEnterCargo);
            return;
        }
        // Inside a different cargo -> exit immediately first, then continue.
        ExitCargoImmediate(/*bSwapAbilities*/false);
    }

    M_PendingTargetCargoActor = TargetCargoActor;

    FVector Entrance = TargetCargo->GetClosestEntranceWorldLocation(
        M_OwningController->GetActorLocation());

    // NEW: ensure the entrance we use as a move target is on the navmesh.
    Entrance = M_OwningController->FindNavigablePointNear(
        Entrance,
        DeveloperSettings::GamePlay::Navigation::SquadUnitCargoEntranceAcceptanceRadius
    );

    // Reset re-path tracking for this EnterCargo command.
    M_RepathToEntranceAttempts = 0;
    M_LastRequestedCargoEntrance = Entrance;

    // Move the whole squad to the entrance as the EnterCargo ability.
    M_OwningController->RequestSquadMoveForAbility(Entrance, EAbilityID::IdEnterCargo);
}



bool UCargoSquad::EnterCargoImmediateInternal(AActor* TargetCargoActor, const bool bSwapAbilities)
{
	static_cast<void>(bSwapAbilities);
	if (not GetIsValidController() || not IsEnterCargoRequestValid(TargetCargoActor))
	{
		return false;
	}

	UCargo* TargetCargo = TargetCargoActor->FindComponentByClass<UCargo>();
	if (not IsValid(TargetCargo))
	{
		return false;
	}

	if (not OnPhysicallyEnteringCargo(TargetCargo))
	{
		return false;
	}
	

	FinalizeEnterCargo(TargetCargo);
	return true;
}
void UCargoSquad::OnEnterCargo_MoveToEntranceCompleted()
{
    if (not GetIsValidController())
    {
        return;
    }

    UCargo* TargetCargo = nullptr;
    if (!ValidatePendingCargoAndResolve(TargetCargo))
    {
        OnEnterCargo_EarlyTerminate();
        return;
    }

    {
        FVector EntranceNow = FVector::ZeroVector;
        if (ShouldRepathToEntrance(TargetCargo, EntranceNow))
        {
            // Safety: avoid infinite loops when units cannot get closer to the entrance.
            ++M_RepathToEntranceAttempts;

            constexpr int32 MaxRepathAttempts = 3;
            if (M_RepathToEntranceAttempts > MaxRepathAttempts)
            {
                RTSFunctionLibrary::ReportError(
                    "UCargoSquad::OnEnterCargo_MoveToEntranceCompleted -> "
                    "Max re-path attempts reached; terminating EnterCargo."
                );
                OnEnterCargo_EarlyTerminate();
                return;
            }

            // Only re-path if the entrance actually moved a meaningful amount.
            const float EntranceShift = FVector::Dist2D(EntranceNow, M_LastRequestedCargoEntrance);
            constexpr float MinEntranceShiftToRepath = 50.f; // tweakable

            if (EntranceShift < MinEntranceShiftToRepath)
            {
                RTSFunctionLibrary::ReportError(
                    "UCargoSquad::OnEnterCargo_MoveToEntranceCompleted -> "
                    "Entrance did not move enough but squad is still outside acceptance. "
                    "Aborting EnterCargo to avoid infinite loop."
                );
                OnEnterCargo_EarlyTerminate();
                return;
            }

            M_LastRequestedCargoEntrance = EntranceNow;
            M_OwningController->RequestSquadMoveForAbility(EntranceNow, EAbilityID::IdEnterCargo);
            return;
        }
    }

    // From here on, no more re-paths: either we're close enough, or we gave up.
    if (!OnPhysicallyEnteringCargo(TargetCargo))
    {
        OnEnterCargo_EarlyTerminate();
        return;
    }

    FinalizeEnterCargo(TargetCargo);
}


void UCargoSquad::ExitCargoImmediate(const bool bSwapAbilities)
{
	if (not GetIsValidController())
	{
		return;
	}
	if (not GetIsInsideCargo() || not GetIsValidCurrentCargo())
	{
		M_SquadCargoState = ESquadCargoState::Free;
		M_CurrentCargo = nullptr;
		M_UnitSeatAssignments.Empty();
		return;
	}

	UCargo* Cargo = M_CurrentCargo.Get();
	const TArray<ASquadUnit*> Units = M_OwningController->GetSquadUnitsChecked();

	// Detach, enable movement, and place near an entrance.
	for (ASquadUnit* Unit : Units)
	{
		DetachUnitEnableMovementAndPlaceNearEntrance(Unit, Cargo);
		Unit->OnUnitEnteredLeftCargo(Cargo, false);
	}

	// Free seats for this squad inside cargo.
	Cargo->OnSquadExited(M_OwningController.Get(), Units);

	M_UnitSeatAssignments.Empty();
	M_CurrentCargo = nullptr;
	M_SquadCargoState = ESquadCargoState::Free;

	if (bSwapAbilities)
	{
		TrySwapAbilities(EAbilityID::IdExitCargo, EAbilityID::IdEnterCargo);
	}
	// Restore nav abilities after exit.
	RestoreNavAbilities_AfterExit();
}

void UCargoSquad::ExecuteExitCargo_Explicit()
{
	if (not GetIsValidController())
	{
		return;
	}
	ExitCargoImmediate(/*bSwapAbilities*/true);
	M_OwningController->DoneExecutingCommand(EAbilityID::IdExitCargo);
}

void UCargoSquad::HandleSeatReassignmentFromCargo(UCargo* CargoComponent, ASquadUnit* UnitToMove,
                                                  const FName& NewSocketName)
{
	if (not IsValid(CargoComponent) || not RTSFunctionLibrary::RTSIsValid(UnitToMove))
	{
		return;
	}
	if (not GetIsInsideCargo() || not GetIsValidCurrentCargo())
	{
		return;
	}

	UCargo* const CurrentCargo = M_CurrentCargo.Get();
	if (CargoComponent != CurrentCargo)
	{
		// Seat change request does not match our current cargo; ignore.
		return;
	}

	AttachUnitAndDisableMovement(UnitToMove, CurrentCargo, NewSocketName);

	// Keep local seat bookkeeping in sync.
	if (FName* ExistingSocket = M_UnitSeatAssignments.Find(UnitToMove))
	{
		*ExistingSocket = NewSocketName;
	}
	else
	{
		M_UnitSeatAssignments.Add(UnitToMove, NewSocketName);
	}
}
void UCargoSquad::CheckCargoState(const EAbilityID NewAbility)
{
	if (not GetIsValidController())
	{
		return;
	}
	if (not GetIsInsideCargo())
	{
		return;
	}
	// If new ability requires being outside then instantly exit (no queue completion).
	if (AbilityRequiresOutside(NewAbility))
	{
		ExitCargoImmediate(/*bSwapAbilities*/true);
	}
}

bool UCargoSquad::AbilityRequiresOutside(const EAbilityID Ability)
{
	switch (Ability)
	{
	// Cargo control and combat can remain inside.
	case EAbilityID::IdEnterCargo:
	case EAbilityID::IdExitCargo:
	case EAbilityID::IdAttack:
	case EAbilityID::IdSwitchWeapon:
	case EAbilityID::IdIdle:
		return false;
	default:
		return true;
	}
}

void UCargoSquad::AttachUnitAndDisableMovement(ASquadUnit* Unit, UCargo* Cargo, const FName& SocketName) const
{
	if (not IsValid(Unit) || not IsValid(Cargo) || not Cargo->GetIsValidCargoMesh())
	{
		return;
	}

	Unit->Force_TerminateMovement();
	if (UCharacterMovementComponent* Move = Unit->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	// Compute desired world transform = (socket world) + (cargo seat offset in world space)
	const FVector UnitOriginalScale = Unit->GetActorScale3D();
	if (not GetIsFiniteVector(UnitOriginalScale))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UCargoSquad::AttachUnitAndDisableMovement -> Unit scale was non-finite."));
		return;
	}

	const FTransform SocketWorld = Cargo->GetSocketWorldTransform(SocketName);
	if (not GetIsFiniteTransform(SocketWorld))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UCargoSquad::AttachUnitAndDisableMovement -> Socket transform was non-finite."));
		return;
	}

	const FVector SeatOffset = Cargo->GetSeatOffset();
	if (not GetIsFiniteVector(SeatOffset))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UCargoSquad::AttachUnitAndDisableMovement -> Seat offset was non-finite."));
		return;
	}

	FTransform DesiredWorld = SocketWorld;
	DesiredWorld.SetLocation(SocketWorld.GetLocation() + SeatOffset);
	DesiredWorld.SetScale3D(UnitOriginalScale);
	if (not GetIsFiniteTransform(DesiredWorld))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UCargoSquad::AttachUnitAndDisableMovement -> Desired world transform was non-finite."));
		return;
	}

	if (UPrimitiveComponent* Mesh = Cargo->GetCargoMesh())
	{
		// First put the unit at the desired world transform…
		Unit->SetActorTransform(DesiredWorld);
		// …then attach while KEEPING that world transform so it stays attached to the socket.
		Unit->AttachToComponent(Mesh, FAttachmentTransformRules::KeepWorldTransform, SocketName);
	}
	else
	{
		// Fallback (no valid mesh to attach to): just place at desired transform.
		Unit->SetActorTransform(DesiredWorld);
	}
}

void UCargoSquad::DetachUnitEnableMovementAndPlaceNearEntrance(ASquadUnit* Unit, UCargo* Cargo) const
{
	if (not IsValid(Unit) || not IsValid(Cargo))
	{
		return;
	}

	// Keep world position when detaching; then place around closest entrance with small jitter.
	Unit->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (UCharacterMovementComponent* Move = Unit->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}

	const FVector ReferenceLocation = GetIsValidController()
		? M_OwningController->GetActorLocation()
		: Unit->GetActorLocation();
	const FVector Base = Cargo->GetClosestEntranceWorldLocation(ReferenceLocation);
	if (not GetIsFiniteVector(Base))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UCargoSquad::DetachUnitEnableMovementAndPlaceNearEntrance -> Base location was non-finite."));
		return;
	}
	// Small randomized ring to avoid overlap when leaving cargo.
	constexpr float ExitPlacementRadius = 75.f;
	constexpr float DegreesInCircle = 360.f;
	const float ExitPlacementAngleDegrees = FMath::FRandRange(0.f, DegreesInCircle);
	const float ExitPlacementAngleRadians = FMath::DegreesToRadians(ExitPlacementAngleDegrees);
	const FVector ExitPlacementOffset = FVector(
		FMath::Cos(ExitPlacementAngleRadians) * ExitPlacementRadius,
		FMath::Sin(ExitPlacementAngleRadians) * ExitPlacementRadius,
		0.f
	);
	const FVector ExitLocationBeforeNavProjection = Base + ExitPlacementOffset;
	if (not GetIsFiniteVector(ExitLocationBeforeNavProjection))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UCargoSquad::DetachUnitEnableMovementAndPlaceNearEntrance -> Exit location was non-finite."));
		return;
	}

	constexpr float ExitNavProjectionRadiusMultiplier = 2.f;
	const float ExitNavProjectionRadius = FMath::Max(
		ExitPlacementRadius,
		DeveloperSettings::GamePlay::Navigation::SquadUnitCargoEntranceAcceptanceRadius *
			ExitNavProjectionRadiusMultiplier
	);
	const FVector ExitLocationProjectedToNavMesh = GetIsValidController()
		? M_OwningController->FindNavigablePointNear(ExitLocationBeforeNavProjection, ExitNavProjectionRadius)
		: ExitLocationBeforeNavProjection;
	if (not GetIsFiniteVector(ExitLocationProjectedToNavMesh))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UCargoSquad::DetachUnitEnableMovementAndPlaceNearEntrance -> Projected exit location was non-finite."));
		return;
	}

	Unit->SetActorLocation(ExitLocationProjectedToNavMesh);
}

void UCargoSquad::TrySwapAbilities(const EAbilityID OldAbility, const EAbilityID NewAbility) const
{
	if (not GetIsValidController())
	{
		return;
	}
	if (not M_OwningController->SwapAbility(OldAbility, NewAbility))
	{
		const FString OldName = Global_GetAbilityIDAsString(OldAbility);
		const FString NewName = Global_GetAbilityIDAsString(NewAbility);
		RTSFunctionLibrary::ReportError(
			"UCargoSquad::TrySwapAbilities -> Failed to swap " + OldName + " -> " + NewName);
	}
}

void UCargoSquad::OnEnterCargo_EarlyTerminate()
{
    // Reset tracking.
    M_RepathToEntranceAttempts = 0;
    M_LastRequestedCargoEntrance = FVector::ZeroVector;
    M_PendingTargetCargoActor = nullptr;

    if (M_OwningController.IsValid())
    {
        M_OwningController->DoneExecutingCommand(EAbilityID::IdEnterCargo);
    }
}

bool UCargoSquad::ValidatePendingCargoAndResolve(UCargo*& OutCargo) const
{
	OutCargo = nullptr;

	AActor* TargetActor = M_PendingTargetCargoActor.Get();
	if (not TargetActor)
	{
		RTSFunctionLibrary::ReportError("OnEnterCargo_MoveToEntranceCompleted -> No pending cargo actor set.");
		return false;
	}

	UCargo* TargetCargo = TargetActor->FindComponentByClass<UCargo>();
	if (not IsValid(TargetCargo))
	{
		RTSFunctionLibrary::ReportError("OnEnterCargo_MoveToEntranceCompleted -> Target has no UCargo.");
		return false;
	}

	OutCargo = TargetCargo;
	return true;
}

bool UCargoSquad::ShouldRepathToEntrance(UCargo* TargetCargo, FVector& OutEntranceNow) const
{
	// Hard-coded path acceptance for cargo via settings, with a sensible floor.
	const float Acceptance = FMath::Max(
		10.f,
		DeveloperSettings::GamePlay::Navigation::SquadUnitCargoEntranceAcceptanceRadius
	);

	OutEntranceNow = TargetCargo->GetClosestEntranceWorldLocation(M_OwningController->GetSquadLocation());

	// Require one unit to be close enough -> use MIN distance of the squad.
	float MinDist = TNumericLimits<float>::Max(); // BUGFIX: was 0.f; that always passes Min()
	const TArray<ASquadUnit*> Units = M_OwningController->GetSquadUnitsChecked();
	for (ASquadUnit* U : Units)
	{
		if (not IsValid(U))
		{
			continue;
		}

		const float D = FVector::Dist(U->GetActorLocation(), OutEntranceNow);
		MinDist = FMath::Min(MinDist, D);
	}

	// If no valid units, be conservative: re-path.
	if (MinDist == TNumericLimits<float>::Max())
	{
		return true;
	}

	return (MinDist > Acceptance);
}

bool UCargoSquad::OnPhysicallyEnteringCargo(UCargo* TargetCargo)
{
	// Ask for seat assignments.
	TMap<ASquadUnit*, FName> Assignments;
	if (not TargetCargo->RequestCanEnterCargo(M_OwningController.Get(), Assignments))
	{
		return false;
	}

	// Attach and disable movement per unit.
	M_UnitSeatAssignments.Empty();
	for (TPair<ASquadUnit*, FName>& It : Assignments)
	{
		ASquadUnit* Unit = It.Key;
		const FName& Socket = It.Value;
		if (not IsValid(Unit))
		{
			continue;
		}
		AttachUnitAndDisableMovement(Unit, TargetCargo, Socket);
		Unit->OnUnitEnteredLeftCargo(TargetCargo, true);
		M_UnitSeatAssignments.Add(Unit, Socket);
	}
	return true;
}

bool UCargoSquad::GetIsFiniteVector(const FVector& Vector) const
{
	return FMath::IsFinite(Vector.X) && FMath::IsFinite(Vector.Y) && FMath::IsFinite(Vector.Z);
}

bool UCargoSquad::GetIsFiniteRotator(const FRotator& Rotator) const
{
	return FMath::IsFinite(Rotator.Pitch)
		&& FMath::IsFinite(Rotator.Yaw)
		&& FMath::IsFinite(Rotator.Roll);
}

bool UCargoSquad::GetIsFiniteTransform(const FTransform& Transform) const
{
	return GetIsFiniteVector(Transform.GetLocation())
		&& GetIsFiniteRotator(Transform.GetRotation().Rotator())
		&& GetIsFiniteVector(Transform.GetScale3D());
}

void UCargoSquad::FinalizeEnterCargo(UCargo* TargetCargo)
{
    if (not M_OwningController.IsValid())
    {
        return;
    }

    // Reset tracking for next time.
    M_RepathToEntranceAttempts = 0;
    M_LastRequestedCargoEntrance = FVector::ZeroVector;

    // Commit current cargo state.
    M_CurrentCargo = TargetCargo;
    M_CurrentCargo->OnSquadEntered(M_OwningController.Get());
    M_SquadCargoState = ESquadCargoState::Inside;

	// Team Weapons cannot swap this ability.
	if(not GetIsOwningSquadATeamWeapon())
	{
    // Swap the UI ability Enter -> Exit.
    TrySwapAbilities(EAbilityID::IdEnterCargo, EAbilityID::IdExitCargo);
	}
    // Hide nav abilities while inside cargo.
    SuspendNavAbilities_WhileInside();

    // Clear pending actor, we are done with this request.
    M_PendingTargetCargoActor = nullptr;
}

void UCargoSquad::SuspendNavAbilities_WhileInside()
{
	if (!GetIsValidController())
	{
		return;
	}

	// Abilities to hide while inside a transport.
	static constexpr EAbilityID ToSuspend[] = {
		EAbilityID::IdMove,
		EAbilityID::IdReverseMove,
		EAbilityID::IdPatrol
	};

	const TArray<EAbilityID> Current = M_OwningController->GetUnitAbilities();

	for (EAbilityID Ability : ToSuspend)
	{
		const int32 Index = Current.IndexOfByKey(Ability);
		if (Index == INDEX_NONE)
		{
			continue; // wasn't present; nothing to remove
		}

		// Remember the original slot (only once) so we can restore in place later.
		if (!M_SuspendedNavAbilityIndices.Contains(Ability))
		{
			M_SuspendedNavAbilityIndices.Add(Ability, Index);
		}

		// Remove by ID (index-agnostic).
		M_OwningController->RemoveAbility(Ability);
	}
}

void UCargoSquad::RestoreNavAbilities_AfterExit()
{
	if (!GetIsValidController())
	{
		return;
	}

	// Put back each ability, try original index first, otherwise use any free slot.
	for (const TPair<EAbilityID, int32>& Pair : M_SuspendedNavAbilityIndices)
	{
		const EAbilityID Ability = Pair.Key;
		const int32 DesiredIndex = Pair.Value;

		// If the ability somehow already came back (e.g. via other logic), skip.
		if (M_OwningController->HasAbility(Ability))
		{
			continue;
		}

		if (!M_OwningController->AddAbility(Ability, DesiredIndex))
		{
			// Index occupied or invalid now -> just add to a free slot.
			M_OwningController->AddAbility(Ability, INDEX_NONE);
			RTSFunctionLibrary::ReportWarning(
				"UCargoSquad::RestoreNavAbilities_AfterExit -> Restored " +
				Global_GetAbilityIDAsString(Ability) +
				" but not at original index " + FString::FromInt(DesiredIndex));
		}
	}

	M_SuspendedNavAbilityIndices.Reset();
}

bool UCargoSquad::GetIsOwningSquadATeamWeapon() const
{
	return M_OwningController.IsValid() && M_OwningController->IsA(ATeamWeaponController::StaticClass());
}
