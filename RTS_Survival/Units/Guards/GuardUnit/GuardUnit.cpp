// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GuardUnit.h"

#include "AIController.h"
#include "GameFramework/Character.h"
#include "RTS_Survival/Units/Guards/GuardMovement/GuardCharacterMovementComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

AGuardUnit::AGuardUnit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UGuardCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
}

bool AGuardUnit::StartGuardMove(const FVector& TargetLocation)
{
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
	}

	if (not GetIsValidGuardMovementComponent())
	{
		return false;
	}

	return M_GuardMovementComponent->StartGuardMove(TargetLocation);
}

void AGuardUnit::StopGuardMove()
{
	if (not GetIsValidGuardMovementComponent())
	{
		return;
	}

	M_GuardMovementComponent->StopGuardMove();
}

void AGuardUnit::SetAutoGuardingEnabled(const bool bEnableAutoGuarding)
{
	bM_IsAutoGuardingEnabled = bEnableAutoGuarding;

	if (not bM_IsAutoGuardingEnabled)
	{
		StopGuardMove();
		return;
	}

	SetWeaponToAutoEngageTargets(false);
}

bool AGuardUnit::GetIsGuardMoveActive() const
{
	if (not GetIsValidGuardMovementComponent())
	{
		return false;
	}

	return M_GuardMovementComponent->GetIsGuardMoveActive();
}

void AGuardUnit::OnSpecificTargetOutOfRange(const FVector& TargetLocation)
{
	if (bM_IsAutoGuardingEnabled)
	{
		return;
	}

	Super::OnSpecificTargetOutOfRange(TargetLocation);
}

void AGuardUnit::OnSpecificTargetDestroyedOrInvisible()
{
	if (bM_IsAutoGuardingEnabled)
	{
		SetWeaponToAutoEngageTargets(false);
		return;
	}

	Super::OnSpecificTargetDestroyedOrInvisible();
}

void AGuardUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (M_GuardMovementComponent != nullptr && M_GuardMoveFinishedHandle.IsValid())
	{
		M_GuardMovementComponent->OnGuardMoveFinished.Remove(M_GuardMoveFinishedHandle);
		M_GuardMoveFinishedHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AGuardUnit::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	M_GuardMovementComponent = Cast<UGuardCharacterMovementComponent>(GetCharacterMovement());
	if (not GetIsValidGuardMovementComponent())
	{
		return;
	}

	if (M_GuardMoveFinishedHandle.IsValid())
	{
		M_GuardMovementComponent->OnGuardMoveFinished.Remove(M_GuardMoveFinishedHandle);
		M_GuardMoveFinishedHandle.Reset();
	}

	M_GuardMoveFinishedHandle = M_GuardMovementComponent->OnGuardMoveFinished.AddUObject(
		this,
		&AGuardUnit::HandleGuardMoveFinished);
}

void AGuardUnit::StrafeToLocation(const FVector& StrafeLocation)
{
	if (not GetIsValidGuardMovementComponent())
	{
		return;
	}

	(void)M_GuardMovementComponent->StartGuardMove(StrafeLocation);
}

void AGuardUnit::TerminateMovementCommand()
{
	StopGuardMove();
	Super::TerminateMovementCommand();
}

bool AGuardUnit::GetIsValidGuardMovementComponent() const
{
	if (IsValid(M_GuardMovementComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_GuardMovementComponent",
		"AGuardUnit::GetIsValidGuardMovementComponent",
		this);
	return false;
}

void AGuardUnit::HandleGuardMoveFinished(const bool bReachedDestination)
{
	OnGuardMoveFinished.Broadcast(this, bReachedDestination);
}
