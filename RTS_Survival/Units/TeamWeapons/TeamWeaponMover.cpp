// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "TeamWeaponMover.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

UTeamWeaponMover::UTeamWeaponMover()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTeamWeaponMover::BeginPlay()
{
	Super::BeginPlay();
}

void UTeamWeaponMover::MoveWeaponToLocation(const FVector& Destination)
{
	M_MoveDestination = Destination;
	if (not bM_IsCrewReady)
	{
		SetMoverState(ETeamWeaponMoverState::AwaitingCrew);
		return;
	}

	if (not GetIsOwnerValid())
	{
		AbortMove(TEXT("Owner missing"));
		return;
	}

	SetMoverState(ETeamWeaponMoverState::Pathing);

	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		AbortMove(TEXT("Owner invalid"));
		return;
	}

	SetMoverState(ETeamWeaponMoverState::Moving);
	OwnerActor->SetActorLocation(M_MoveDestination);
	SetMoverState(ETeamWeaponMoverState::Idle);
	OnMoverArrived.Broadcast();
}

void UTeamWeaponMover::AbortMove(const FString& Reason)
{
	SetMoverState(ETeamWeaponMoverState::Idle);
	OnMoverFailed.Broadcast(Reason);
}

void UTeamWeaponMover::NotifyCrewReady(const bool bIsReady)
{
	bM_IsCrewReady = bIsReady;
	if (bM_IsCrewReady && M_MoverState == ETeamWeaponMoverState::AwaitingCrew)
	{
		MoveWeaponToLocation(M_MoveDestination);
	}
}

bool UTeamWeaponMover::GetIsMoving() const
{
	return M_MoverState == ETeamWeaponMoverState::Moving;
}

void UTeamWeaponMover::SetMoverState(const ETeamWeaponMoverState NewState)
{
	if (M_MoverState == NewState)
	{
		return;
	}
	M_MoverState = NewState;
	OnMoverStateChanged.Broadcast(NewState);
}

bool UTeamWeaponMover::GetIsOwnerValid() const
{
	if (IsValid(GetOwner()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("Owner"), TEXT("UTeamWeaponMover::GetIsOwnerValid"),
		                                                     GetOwner());
	return false;
}
