// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "TeamWeaponMover.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UTeamWeaponMover::UTeamWeaponMover()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTeamWeaponMover::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(false);
}

void UTeamWeaponMover::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (M_MoverState != ETeamWeaponMoverState::Moving)
	{
		return;
	}

	UpdateOwnerLocationFromCrew();

	if (not HaveCrewReachedDestination())
	{
		return;
	}

	SetComponentTickEnabled(false);
	bM_HasMoveRequest = false;
	SetMoverState(ETeamWeaponMoverState::Idle);
	OnMoverArrived.Broadcast();
}

void UTeamWeaponMover::SetCrewMembersToFollow(const TArray<FTeamWeaponCrewMemberOffset>& CrewMembers)
{
	M_CrewMembers = CrewMembers;
}

void UTeamWeaponMover::MoveWeaponToLocation(const FVector& Destination)
{
	M_MoveDestination = Destination;
	bM_HasMoveRequest = true;
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

	if (not GetIsCrewDataValid())
	{
		AbortMove(TEXT("Crew data invalid"));
		return;
	}

	SetMoverState(ETeamWeaponMoverState::Pathing);
}

void UTeamWeaponMover::BeginFollowingCrew()
{
	if (not bM_HasMoveRequest)
	{
		return;
	}

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

	if (not GetIsCrewDataValid())
	{
		AbortMove(TEXT("Crew data invalid"));
		return;
	}

	SetComponentTickEnabled(true);
	SetMoverState(ETeamWeaponMoverState::Moving);
	UpdateOwnerLocationFromCrew();
}

void UTeamWeaponMover::AbortMove(const FString& Reason)
{
	SetComponentTickEnabled(false);
	bM_HasMoveRequest = false;
	SetMoverState(ETeamWeaponMoverState::Idle);
	OnMoverFailed.Broadcast(Reason);
}

void UTeamWeaponMover::NotifyCrewReady(const bool bIsReady)
{
	bM_IsCrewReady = bIsReady;
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

bool UTeamWeaponMover::GetIsCrewDataValid() const
{
	if (M_CrewMembers.Num() > 0)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_CrewMembers"),
	                                                      TEXT("UTeamWeaponMover::GetIsCrewDataValid"), this);
	return false;
}

bool UTeamWeaponMover::TryGetCrewCenterLocation(FVector& OutCenter) const
{
	int32 ValidCrewCount = 0;
	FVector AccumulatedLocation = FVector::ZeroVector;

	for (const FTeamWeaponCrewMemberOffset& CrewEntry : M_CrewMembers)
	{
		if (not CrewEntry.M_CrewMember.IsValid())
		{
			continue;
		}

		AccumulatedLocation += CrewEntry.M_CrewMember->GetActorLocation();
		ValidCrewCount++;
	}

	if (ValidCrewCount == 0)
	{
		return false;
	}

	OutCenter = AccumulatedLocation / static_cast<float>(ValidCrewCount);
	return true;
}

bool UTeamWeaponMover::HaveCrewReachedDestination() const
{
	const float AcceptanceRadius = DeveloperSettings::GamePlay::Navigation::SquadUnitAcceptanceRadius;
	const float AcceptanceDistanceSquared = FMath::Square(AcceptanceRadius);

	bool bHasValidCrewMember = false;
	for (const FTeamWeaponCrewMemberOffset& CrewEntry : M_CrewMembers)
	{
		if (not CrewEntry.M_CrewMember.IsValid())
		{
			continue;
		}

		bHasValidCrewMember = true;
		const FVector ExpectedLocation = M_MoveDestination + CrewEntry.M_OffsetFromCenter;
		const float DistanceSquared = FVector::DistSquared(CrewEntry.M_CrewMember->GetActorLocation(), ExpectedLocation);
		if (DistanceSquared > AcceptanceDistanceSquared)
		{
			return false;
		}
	}

	if (not bHasValidCrewMember)
	{
		return false;
	}

	return true;
}

void UTeamWeaponMover::UpdateOwnerLocationFromCrew()
{
	if (not GetIsOwnerValid())
	{
		AbortMove(TEXT("Owner missing"));
		return;
	}

	FVector CrewCenter = FVector::ZeroVector;
	if (not TryGetCrewCenterLocation(CrewCenter))
	{
		AbortMove(TEXT("Crew missing"));
		return;
	}

	AActor* OwnerActor = GetOwner();
	OwnerActor->SetActorLocation(CrewCenter);
}
