// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "TeamWeaponController.h"

#include "TeamWeapon.h"
#include "TeamWeaponMover.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"

ATeamWeaponController::ATeamWeaponController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATeamWeaponController::BeginPlay()
{
	Super::BeginPlay();
	SetTeamWeaponState(ETeamWeaponState::Spawning);
}

void ATeamWeaponController::OnAllSquadUnitsLoaded()
{
	Super::OnAllSquadUnitsLoaded();
	SpawnTeamWeapon();
}

void ATeamWeaponController::ExecuteMoveCommand(const FVector MoveToLocation)
{
	if (not GetIsValidTeamWeapon() || not GetIsValidTeamWeaponMover())
	{
		Super::ExecuteMoveCommand(MoveToLocation);
		return;
	}

	SetTeamWeaponState(ETeamWeaponState::Moving);
	M_TeamWeaponMover->MoveWeaponToLocation(MoveToLocation);
}

void ATeamWeaponController::ExecutePatrolCommand(const FVector PatrolToLocation)
{
	DoneExecutingCommand(EAbilityID::IdPatrol);
}

void ATeamWeaponController::ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand)
{
	Super::ExecuteRotateTowardsCommand(RotateToRotator, IsQueueCommand);
}

void ATeamWeaponController::UnitInSquadDied(ASquadUnit* UnitDied, bool bUnitSelected,
                                            const ERTSDeathType DeathType)
{
	Super::UnitInSquadDied(UnitDied, bUnitSelected, DeathType);
	AssignCrewToTeamWeapon();
}

void ATeamWeaponController::OnSquadUnitCommandComplete(EAbilityID CompletedAbilityID)
{
	const UCommandData* CommandData = GetIsValidCommandData();
	if (CommandData && CommandData->GetCurrentActiveCommand() == EAbilityID::IdMove)
	{
		return;
	}
	Super::OnSquadUnitCommandComplete(CompletedAbilityID);
}

void ATeamWeaponController::SpawnTeamWeapon()
{
	if (M_TeamWeapon)
	{
		return;
	}
	if (not TeamWeaponClass)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("TeamWeaponClass"),
			                                                  TEXT("ATeamWeaponController::SpawnTeamWeapon"), this);
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("World not valid in ATeamWeaponController::SpawnTeamWeapon");
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_TeamWeapon = World->SpawnActor<ATeamWeapon>(TeamWeaponClass, GetActorLocation(), GetActorRotation(),
	                                             SpawnParameters);
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	M_TeamWeapon->SetTeamWeaponController(this);
	M_TeamWeapon->SetTurretOwnerActor(this);
	M_TeamWeaponMover = M_TeamWeapon->GetTeamWeaponMover();

	if (GetIsValidTeamWeaponMover())
	{
		M_TeamWeaponMover->OnMoverArrived.AddUObject(this, &ATeamWeaponController::HandleMoverArrived);
		M_TeamWeaponMover->OnMoverFailed.AddUObject(this, &ATeamWeaponController::HandleMoverFailed);
	}

	AssignCrewToTeamWeapon();
	SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
}

void ATeamWeaponController::AssignCrewToTeamWeapon()
{
	M_CrewAssignment.Reset();
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	const int32 RequiredOperators = M_TeamWeapon->GetRequiredOperators();
	const TArray<ASquadUnit*> SquadUnits = GetSquadUnitsChecked();
	M_CrewAssignment.M_RequiredOperators = RequiredOperators;

	int32 OperatorIndex = 0;
	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		if (OperatorIndex < RequiredOperators)
		{
			M_CrewAssignment.M_Operators.Add(SquadUnit);
			OperatorIndex++;

			AInfantryWeaponMaster* InfantryWeapon = SquadUnit->GetInfantryWeapon();
			if (IsValid(InfantryWeapon))
			{
				InfantryWeapon->DisableWeaponSearch(false, true);
			}
			continue;
		}

		M_CrewAssignment.M_Guards.Add(SquadUnit);
	}

	if (GetIsValidTeamWeaponMover())
	{
		M_TeamWeaponMover->NotifyCrewReady(M_CrewAssignment.GetHasEnoughOperators());
	}
}

void ATeamWeaponController::HandleMoverArrived()
{
	SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
	DoneExecutingCommand(EAbilityID::IdMove);
}

void ATeamWeaponController::HandleMoverFailed(const FString& FailureReason)
{
	SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
	RTSFunctionLibrary::ReportError("Team weapon mover failed: " + FailureReason);
	DoneExecutingCommand(EAbilityID::IdMove);
}

void ATeamWeaponController::SetTeamWeaponState(const ETeamWeaponState NewState)
{
	if (M_TeamWeaponState == NewState)
	{
		return;
	}
	M_TeamWeaponState = NewState;
}

bool ATeamWeaponController::GetIsValidTeamWeapon() const
{
	if (IsValid(M_TeamWeapon))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TeamWeapon",
	                                                      "ATeamWeaponController::GetIsValidTeamWeapon", this);
	return false;
}

bool ATeamWeaponController::GetIsValidTeamWeaponMover() const
{
	if (M_TeamWeaponMover.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TeamWeaponMover",
	                                                      "ATeamWeaponController::GetIsValidTeamWeaponMover", this);
	return false;
}

int ATeamWeaponController::GetOwningPlayer()
{
	if (GetIsValidRTSComponent())
	{
		return RTSComponent->GetOwningPlayer();
	}
	return -1;
}

void ATeamWeaponController::OnTurretOutOfRange(const FVector TargetLocation, ACPPTurretsMaster* CallingTurret)
{
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	if (M_TeamWeaponState != ETeamWeaponState::Ready_Deployed)
	{
		return;
	}

	ExecuteMoveCommand(TargetLocation);
}

void ATeamWeaponController::OnTurretInRange(ACPPTurretsMaster* CallingTurret)
{
	if (not GetIsValidTeamWeapon())
	{
		return;
	}
}

void ATeamWeaponController::OnMountedWeaponTargetDestroyed(
	ACPPTurretsMaster* CallingTurret,
	UHullWeaponComponent* CallingHullWeapon,
	AActor* DestroyedActor,
	const bool bWasDestroyedByOwnWeapons)
{
}

void ATeamWeaponController::OnFireWeapon(ACPPTurretsMaster* CallingTurret)
{
}

void ATeamWeaponController::OnProjectileHit(const bool bBounced)
{
}

FRotator ATeamWeaponController::GetOwnerRotation() const
{
	return GetActorRotation();
}
